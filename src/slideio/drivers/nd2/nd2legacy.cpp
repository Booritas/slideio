// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/nd2/nd2legacy.hpp"
#include "slideio/drivers/nd2/nd2internal.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/log.hpp"
#include "slideio/core/tools/filereader.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <unordered_map>

using namespace slideio;
using namespace slideio::nd2;
using namespace slideio::nd2::detail;

namespace
{
    const char kMapSignature[] = "LABORATORY IMAGING ND BOX MAP 00";
    constexpr uint64_t kSignatureSize = 32;
    constexpr uint64_t kTrailerSize = 40;      // signature + u64 distance from EOF to the map
    constexpr uint64_t kMapEntrySize = 16;     // JP2 type, LIM type, u64 LE offset
    constexpr uint64_t kIhdrSize = 14;
    constexpr double kMicro = 1e-6;
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    /// A VIMD box keeps its time near the start; this much is read before the whole box.
    constexpr uint64_t kTimeProbeBytes = 4096;

    [[noreturn]] void damaged(const std::string& what)
    {
        RAISE_RUNTIME_ERROR << "ND2: " << what;
    }

    uint16_t be16(const uint8_t* p) { return static_cast<uint16_t>((p[0] << 8) | p[1]); }
    uint32_t be32(const uint8_t* p)
    {
        return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
    }
    uint64_t be64(const uint8_t* p) { return (uint64_t(be32(p)) << 32) | be32(p + 4); }
    uint64_t le64(const uint8_t* p)
    {
        uint64_t v = 0;
        for (int i = 7; i >= 0; --i) {
            v = (v << 8) | p[i];
        }
        return v;
    }

    std::string printable(const uint8_t* p, size_t n)
    {
        std::string s;
        for (size_t i = 0; i < n; ++i) {
            s += (p[i] >= 0x20 && p[i] < 0x7F) ? static_cast<char>(p[i]) : '?';
        }
        return s;
    }

    /// Walks the boxes in [begin, end) of the file; appends them to `out`.
    void walkBoxes(const FileReader& file, uint64_t begin, uint64_t end, std::vector<Box>& out)
    {
        uint64_t offset = begin;
        while (offset < end) {
            const uint64_t left = end - offset;
            if (left < 8) {
                damaged("legacy box at offset " + std::to_string(offset) + " is truncated");
            }
            uint8_t header[16];
            file.readAt(offset, header, 8);
            uint64_t length = be32(header);
            uint64_t headerSize = 8;
            if (length == 1) {
                if (left < 16) {
                    damaged("legacy box at offset " + std::to_string(offset) + " is truncated");
                }
                file.readAt(offset + 8, header + 8, 8);
                length = be64(header + 8);
                headerSize = 16;
            }
            else if (length == 0) {
                length = left;   // the box runs to the end
            }
            const std::string type = printable(header + 4, 4);
            if (length < headerSize || length > left) {
                damaged("legacy box '" + type + "' at offset " + std::to_string(offset) + " declares "
                    + std::to_string(length) + " bytes, but " + std::to_string(left) + " remain");
            }
            Box box;
            box.offset = offset;
            box.dataOffset = offset + headerSize;
            box.dataLength = length - headerSize;
            box.type = type;
            out.push_back(std::move(box));
            offset += length;
        }
    }

    // ------------------------------------------------------------------ metadata
    //
    // Real legacy boxes use the chunked, prefixed names (`eType`, `uLoopPars`, `uiCount`,
    // `sPicturePlanes`, `dCalibration`); the nd2 package and the fixtures use them without
    // the prefix (`Type`, `LoopPars`, `Count`). detail::fieldOf accepts both spellings, so
    // every lookup below names the prefixed field.

    double number(const Variant* v, const std::string& name, double fallback)
    {
        const Variant* f = fieldOf(v, name);
        return f ? f->asDouble(fallback) : fallback;
    }

    /// The legacy schema of a loop record (FD 11.2): XY points in `dPosX`, and the child
    /// either the `ppNextLevelEx` record itself or the elements of that list.
    LoopRecord readLegacyRecord(const Variant& node, int64_t maxFrames)
    {
        LoopRecord r;
        const Variant* typeField = fieldOf(&node, "eType");
        r.type = typeField ? static_cast<int>(typeField->asInt(0)) : 0;
        if (const Variant* pars = fieldOf(&node, "uLoopPars")) {
            r.hasPars = true;
            r.loop.pars = unwrapPars(*pars);
            countLoop(r.type, fieldOf(&r.loop.pars, "dPosX"), fieldOf(&node, "pItemValid"), r.loop, maxFrames);
        }
        const Variant* next = fieldOf(&node, "ppNextLevelEx");
        if (next && fieldOf(next, "eType")) {
            r.children.push_back(next);
        }
        else {
            r.children = elementsOf(next);
        }
        return r;
    }

    /// AIM1/AIMD: the loop record under its wrappers (`vMetadata`, `no_name`), or the old
    /// `LoopNo00`, `LoopNo01`... list, one loop per level.
    std::vector<RawLoop> readLoops(const Variant& aim, int64_t maxFrames)
    {
        const Variant* node = &aim;
        for (int i = 0; i < kMaxLoopDepth && !fieldOf(node, "eType"); ++i) {
            if (node->get("LoopNo00")) {
                break;
            }
            if (node->kind() == Variant::Kind::List && node->items().size() == 1) {
                node = &node->items().front();
                continue;
            }
            std::vector<const Variant*> inner;
            for (const auto& m : node->members()) {
                if (m.first != "UnknownData" && (m.second.kind() == Variant::Kind::Map
                    || m.second.kind() == Variant::Kind::List)) {
                    inner.push_back(&m.second);
                }
            }
            if (inner.size() != 1) {
                break;
            }
            node = inner.front();
        }
        const LoopRecordReader read = [maxFrames](const Variant& n) { return readLegacyRecord(n, maxFrames); };
        std::vector<RawLoop> raw;
        if (node->get("LoopNo00")) {
            int level = 0;
            for (const auto& m : node->members()) {
                if (m.first.compare(0, 6, "LoopNo") == 0) {
                    flattenLoops(unwrapPars(m.second), read, level++, raw);
                }
            }
        }
        else if (fieldOf(node, "eType")) {
            flattenLoops(*node, read, 0, raw);
        }
        return raw;
    }

    /// tinfMetadataText, `depth` levels down.
    std::string metadataTextImpl(const Variant& v, int depth)
    {
        if (depth > kMaxLoopDepth) {
            return std::string();
        }
        for (const auto& m : v.members()) {
            if (m.second.kind() == Variant::Kind::String && m.first == "Text"
                && m.second.asString().compare(0, 9, "Metadata:") == 0) {
                return m.second.asString();
            }
            const std::string found = metadataTextImpl(m.second, depth + 1);
            if (!found.empty()) {
                return found;
            }
        }
        // An indexed Map's items() are its members again: walking both would visit
        // each child twice per level, 2^depth in all.
        if (v.kind() == Variant::Kind::List) {
            for (const Variant& item : v.items()) {
                const std::string found = metadataTextImpl(item, depth + 1);
                if (!found.empty()) {
                    return found;
                }
            }
        }
        return std::string();
    }

    /// The shared loop rules, then FD 11.3: the TINF Dimensions XY count rules over the XY
    /// loop's when they differ (R22: when they agree, the flagged points are kept). The
    /// loops, the text count included, may declare at most `maxFrames` frames (design 4.5).
    Experiment buildExperiment(const Variant* aim, int64_t textXY, const std::string& fileStem,
        int64_t maxFrames)
    {
        std::vector<Loop> loops = axisLoops(aim ? readLoops(*aim, maxFrames) : std::vector<RawLoop>());
        for (Loop& loop : loops) {
            if (loop.axis == Axis::P && textXY > 0 && textXY != loop.count) {
                if (textXY > maxFrames) {
                    throw Unsupported("ND2: unsupported experiment (the text info says XY(" + std::to_string(textXY)
                        + "), more than the " + std::to_string(maxFrames) + " frames the file can justify)");
                }
                SLIDEIO_LOG(INFO) << "ND2: '" << fileStem << "': the XY loop has " << loop.count
                    << " valid points; the text info says XY(" << textXY << "), which is used";
                loop.count = textXY;
                loop.validItems.clear();
                for (int64_t i = 0; i < textXY; ++i) {
                    loop.validItems.push_back(i);
                }
            }
        }
        return Experiment::fromLoops(std::move(loops), maxFrames);
    }

    /// `TimeMSec` of a VIMD box, read from its text without parsing the whole box (each
    /// frame has one, ~24 KB in the samples). NaN when the box has none.
    double frameTime(const LegacyFile& legacy, const FileReader& file, const Box& box)
    {
        auto scan = [](const std::string& text, double& out) {
            static const std::string key = "TimeMSec";
            for (size_t at = text.find(key); at != std::string::npos; at = text.find(key, at + 1)) {
                const size_t end = at + key.size();
                if (end >= text.size() || (text[end] != ' ' && text[end] != '/' && text[end] != '>')) {
                    continue;
                }
                size_t start = at;
                while (start > 0 && ((text[start - 1] >= 'a' && text[start - 1] <= 'z') || text[start - 1] == '_')) {
                    --start;
                }
                if (start == 0 || text[start - 1] != '<') {
                    continue;
                }
                const size_t close = text.find('>', end);
                const size_t value = text.find("value=\"", end);
                if (value == std::string::npos || (close != std::string::npos && value > close)) {
                    return false;
                }
                const size_t q = text.find('"', value + 7);
                if (q == std::string::npos) {
                    return false;
                }
                std::istringstream in(text.substr(value + 7, q - value - 7));
                in.imbue(std::locale::classic());
                double v = 0;
                in >> v;
                if (in.fail()) {
                    return false;
                }
                out = v;
                return true;
            }
            return false;
        };
        double t = kNaN;
        const uint64_t probe = std::min<uint64_t>(box.dataLength, kTimeProbeBytes);
        std::string text(static_cast<size_t>(probe), '\0');
        if (probe > 0) {
            file.readAt(box.dataOffset, &text[0], text.size());
        }
        if (scan(text, t)) {
            return t;
        }
        if (box.dataLength > probe) {
            const std::vector<uint8_t> all = legacy.readBox(file, box);
            if (scan(std::string(all.begin(), all.end()), t)) {
                return t;
            }
        }
        return kNaN;
    }

    std::vector<Position> buildPositions(const Experiment& experiment, const std::string& fileStem)
    {
        std::vector<Position> out;
        const Loop* xy = experiment.loop(Axis::P);
        if (!xy) {
            out.push_back(Position{ fileStem, std::string() });
            return out;
        }
        const char* const axes[] = { "dPosX", "dPosY", "dPosZ", "dPFSOffset" };
        for (size_t k = 0; k < xy->validItems.size(); ++k) {
            const size_t item = static_cast<size_t>(xy->validItems[k]);
            Position p;
            p.name = "Position " + std::to_string(k + 1);
            nlohmann::json point = nlohmann::json::object();
            for (const char* axis : axes) {
                const std::vector<const Variant*> values = elementsOf(fieldOf(&xy->pars, axis));
                if (item < values.size()) {
                    point[unprefixed(axis)] = values[item]->toJson();
                }
            }
            if (!point.empty()) {
                p.rawJson = point.dump();
            }
            out.push_back(std::move(p));
        }
        return out;
    }
}

// ------------------------------------------------------------------ LegacyFile

LegacyFile LegacyFile::read(const FileReader& file)
{
    LegacyFile legacy;
    const uint64_t size = file.size();
    walkBoxes(file, 0, size, legacy.m_boxes);

    // The box map (FD 11.2).
    if (size < kTrailerSize) {
        damaged("legacy box map: the file is too short for the trailer");
    }
    uint8_t trailer[kTrailerSize];
    file.readAt(size - kTrailerSize, trailer, sizeof(trailer));
    if (std::memcmp(trailer, kMapSignature, kSignatureSize) != 0) {
        damaged("legacy box map: no map signature 40 bytes before the end of the file");
    }
    const uint64_t distance = le64(trailer + kSignatureSize);
    if (distance < kTrailerSize + 4 || distance > size) {
        damaged("legacy box map: the map distance " + std::to_string(distance) + " is outside the file");
    }
    const uint64_t mapStart = size - distance;
    uint8_t countBytes[4];
    file.readAt(mapStart, countBytes, sizeof(countBytes));
    const uint64_t count = be32(countBytes);
    if (count > (distance - kTrailerSize - 4) / kMapEntrySize) {
        damaged("legacy box map: " + std::to_string(count) + " entries do not fit in the map");
    }
    std::vector<uint8_t> entries(static_cast<size_t>(count * kMapEntrySize));
    if (!entries.empty()) {
        file.readAt(mapStart + 4, entries.data(), entries.size());
    }
    std::unordered_map<uint64_t, size_t> byOffset;
    for (size_t i = 0; i < legacy.m_boxes.size(); ++i) {
        byOffset.emplace(legacy.m_boxes[i].offset, i);
    }
    for (uint64_t i = 0; i < count; ++i) {
        const uint8_t* e = entries.data() + i * kMapEntrySize;
        const uint64_t offset = le64(e + 8);
        const auto it = byOffset.find(offset);
        if (it == byOffset.end()) {
            damaged("legacy box map entry " + std::to_string(i) + " points at offset " + std::to_string(offset)
                + ", which is not the start of a box");
        }
        Box& box = legacy.m_boxes[it->second];
        const std::string type = printable(e, 4);
        if (type != box.type) {
            damaged("legacy box map entry " + std::to_string(i) + " says '" + type + "' for the box at offset "
                + std::to_string(offset) + ", which is '" + box.type + "'");
        }
        box.limType = printable(e + 4, 4);
    }

    for (const Box& box : legacy.m_boxes) {
        if (box.type == "jp2c") {
            legacy.m_codestreams.push_back(box);
        }
    }

    // jp2h/ihdr (FD 11.1).
    const auto jp2h = std::find_if(legacy.m_boxes.begin(), legacy.m_boxes.end(),
        [](const Box& b) { return b.type == "jp2h"; });
    if (jp2h == legacy.m_boxes.end()) {
        damaged("legacy file has no jp2h header box");
    }
    std::vector<Box> header;
    walkBoxes(file, jp2h->dataOffset, jp2h->dataOffset + jp2h->dataLength, header);
    if (header.empty() || header.front().type != "ihdr" || header.front().dataLength < kIhdrSize) {
        damaged("legacy file has no ihdr box at the start of jp2h");
    }
    uint8_t ihdr[kIhdrSize];
    file.readAt(header.front().dataOffset, ihdr, sizeof(ihdr));
    const uint32_t height = be32(ihdr);
    const uint32_t width = be32(ihdr + 4);
    const uint16_t components = be16(ihdr + 8);
    if (width == 0 || height == 0 || width > uint32_t(std::numeric_limits<int>::max())
        || height > uint32_t(std::numeric_limits<int>::max()) || components == 0) {
        damaged("legacy ihdr gives a " + std::to_string(width) + "x" + std::to_string(height) + " image with "
            + std::to_string(components) + " components");
    }
    legacy.m_width = static_cast<int>(width);
    legacy.m_height = static_cast<int>(height);
    legacy.m_components = components;
    legacy.m_bits = (ihdr[10] & 0x7F) + 1;
    legacy.m_signed = (ihdr[10] & 0x80) != 0;
    return legacy;
}

std::vector<uint8_t> LegacyFile::readBox(const FileReader& file, const Box& box, uint64_t maxBytes) const
{
    if (box.dataLength > maxBytes || box.dataLength > std::numeric_limits<size_t>::max()) {
        damaged("legacy box at offset " + std::to_string(box.offset) + " declares "
            + std::to_string(box.dataLength) + " bytes, more than it may hold");
    }
    std::vector<uint8_t> data(static_cast<size_t>(box.dataLength));
    if (!data.empty()) {
        file.readAt(box.dataOffset, data.data(), data.size());
    }
    return data;
}

const Box* LegacyFile::first(const std::string& limType) const
{
    for (const Box& box : m_boxes) {
        if (box.limType == limType) {
            return &box;
        }
    }
    return nullptr;
}

std::vector<const Box*> LegacyFile::all(const std::string& limType) const
{
    std::vector<const Box*> out;
    for (const Box& box : m_boxes) {
        if (box.limType == limType) {
            out.push_back(&box);
        }
    }
    return out;
}

// ------------------------------------------------------------------- functions

std::string slideio::nd2::tinfMetadataText(const Variant& tinf)
{
    return metadataTextImpl(tinf, 0);
}

int64_t slideio::nd2::xyCountFromText(const std::string& textInfo)
{
    const size_t at = textInfo.find("Dimensions:");
    if (at == std::string::npos) {
        return -1;
    }
    const size_t end = textInfo.find_first_of("\r\n", at);
    const std::string line = textInfo.substr(at, end == std::string::npos ? std::string::npos : end - at);
    // name(count) pairs, the name optionally followed by a prime: T'(81) x XY(5) x lambda(2)
    for (size_t open = line.find('('); open != std::string::npos; open = line.find('(', open + 1)) {
        size_t nameEnd = open;
        if (nameEnd > 0 && line[nameEnd - 1] == '\'') {
            --nameEnd;
        }
        size_t nameStart = nameEnd;
        while (nameStart > 0 && std::isalnum(static_cast<unsigned char>(line[nameStart - 1]))) {
            --nameStart;
        }
        if (line.compare(nameStart, nameEnd - nameStart, "XY") != 0 || nameEnd - nameStart != 2) {
            continue;
        }
        int64_t n = 0;
        size_t i = open + 1;
        for (; i < line.size() && line[i] >= '0' && line[i] <= '9'; ++i) {
            if (n > (std::numeric_limits<int64_t>::max() - 9) / 10) {
                return -1;
            }
            n = n * 10 + (line[i] - '0');
        }
        if (i == open + 1 || i >= line.size() || line[i] != ')') {
            continue;
        }
        return n;
    }
    return -1;
}

ImageInfo slideio::nd2::buildImageInfo(const LegacyFile& legacy, const FileReader& file,
    const std::string& fileStem)
{
    ImageInfo info;
    nlohmann::json raw = nlohmann::json::object();
    raw["version"] = "1.0";

    // The first box of a LIM type, decoded; `toRaw` adds it to the raw metadata.
    auto load = [&](const std::string& limType, Variant& out, bool toRaw) {
        const Box* box = legacy.first(limType);
        if (!box) {
            return false;
        }
        out = decodeLegacyXml(legacy.readBox(file, *box));
        if (toRaw) {
            raw[limType] = out.toJson();
        }
        return true;
    };
    Variant vimd, vcal, aim, tinf, acal, artt;
    const bool hasVimd = load("VIMD", vimd, true);
    const bool hasVcal = load("VCAL", vcal, true);
    const bool hasAim = load("AIM1", aim, true) || load("AIMD", aim, true);
    const bool hasTinf = load("TINF", tinf, true);
    info.rawJson = raw.dump();
    const bool hasAcal = !hasVcal && load("ACAL", acal, false);
    const bool hasArtt = load("ARTT", artt, false);

    // Sample type from ihdr (FD 11.3).
    // A codestream narrower than 16 bits (a 12-bit camera) is read as 16-bit samples, with
    // the true depth as the significant bits (PR review).
    if (legacy.signedSamples() || legacy.bits() < 1 || legacy.bits() > 16) {
        throw Unsupported("ND2: unsupported legacy sample type: ihdr gives " + std::to_string(legacy.bits())
            + "-bit " + (legacy.signedSamples() ? "signed" : "unsigned") + " samples (expected 1 to 16 bit unsigned)");
    }
    // Channels from VIMD.PicturePlanes.Count of frame 0, never from the spectral loop.
    int64_t channelCount = pictureChannelCount(hasVimd ? &vimd : nullptr);
    if (channelCount <= 0) {
        channelCount = 1;
    }
    const int perChannel = legacy.components();
    if (channelCount > std::numeric_limits<int>::max() / perChannel) {
        throw Unsupported("ND2: unsupported channel layout: " + std::to_string(channelCount) + " channels");
    }

    Attributes& a = info.attributes;
    a.width = legacy.width();
    a.height = legacy.height();
    a.components = static_cast<int>(channelCount) * perChannel;
    a.bitsInMemory = legacy.bits() <= 8 ? 8 : 16;
    const Variant* significantField = hasArtt ? fieldOf(&artt, "SignificantBits") : nullptr;
    const int64_t significant = significantField ? significantField->asInt(0) : 0;
    a.significantBits = (significant > 0 && significant <= a.bitsInMemory) ? static_cast<int>(significant) : legacy.bits();
    a.rowStride = static_cast<int64_t>(a.width) * a.components * a.sampleBytes();
    a.compression = FrameCompression::None;
    a.sequenceCount = static_cast<int64_t>(legacy.all("VCAL").size());
    // Without plane information the file is one channel of ihdr-components samples, one
    // codestream per frame, as the frame source reads it.
    info.channels = parseChannels(hasVimd ? &vimd : nullptr, a.components, channelCount);
    // The nd2 package names a legacy channel by its optical configuration; real files
    // also carry sDescription, the chunked name, which parseChannels prefers.
    const Variant* planes = fieldOf(hasVimd ? &vimd : nullptr, "sPicturePlanes");
    const Variant* records = fieldOf(planes, "sPlaneNew");
    const std::vector<const Variant*> planeRecords = elementsOf(records ? records : fieldOf(planes, "sPlane"));
    for (ChannelInfo& ch : info.channels) {
        const size_t c = static_cast<size_t>(ch.nd2Channel);
        if (ch.name.empty() && c < planeRecords.size()) {
            if (const Variant* name = fieldOf(planeRecords[c], "sOpticalConfigName")) {
                ch.name = name->asString();
            }
        }
    }

    // Dimensions (FD 11.3): the AIM loops, with the XY count from the TINF text.
    const int64_t textXY = hasTinf ? xyCountFromText(tinfMetadataText(tinf)) : -1;
    const int64_t streams = static_cast<int64_t>(legacy.codestreams().size());
    // Design 4.5 (R25): a frame counts toward T only when the codestreams of all its
    // channels exist. A partial last frame stays readable inside T (its missing channels
    // read as background) but never adds a time point.
    const int64_t completeFrames = streams / channelCount;
    // Design 4.5: the loops may declare at most maxDeclaredFrames(present) frames.
    info.experiment = buildExperiment(hasAim ? &aim : nullptr, textXY, fileStem,
        maxDeclaredFrames(completeFrames));
    std::set<int64_t> present;
    for (int64_t f = 0; f < completeFrames; ++f) {
        present.insert(f);
    }
    // Frame times: TimeMSec of each frame's VIMD, else NaN. Read as far as a fitted T
    // could reach (R31), so surplus time points can be checked against them (R29).
    const std::vector<const Box*> vimds = legacy.all("VIMD");
    const int64_t reach = info.experiment.loopProduct() + static_cast<int64_t>(present.size());
    std::vector<double> times(static_cast<size_t>(reach), kNaN);
    for (int64_t f = 0; f < reach && f < static_cast<int64_t>(vimds.size()); ++f) {
        times[static_cast<size_t>(f)] = f == 0
            ? number(&vimd, "dTimeMSec", kNaN) : frameTime(legacy, file, *vimds[static_cast<size_t>(f)]);
    }
    info.experiment.fitTime(present, times);
    times.resize(static_cast<size_t>(info.experiment.loopProduct()), kNaN);
    info.frameTimesMs = std::move(times);
    info.positions = buildPositions(info.experiment, fileStem);

    // Calibration (design 6.3): VCAL of frame 0, else ACAL; 0 when uncalibrated or <= 0.
    if (hasVcal) {
        const Variant* flag = fieldOf(&vcal, "bCalibrated");
        if (!flag) {
            flag = fieldOf(&vcal, "bValid");
        }
        const double um = number(&vcal, "dCalibration", 0);
        if (flag && flag->asBool(false) && um > 0) {
            info.pixelSizeX = um * kMicro;
            info.pixelSizeY = um * kMicro;
        }
        const double aspect = number(&vcal, "dAspect", 1);
        if (aspect != 1) {
            SLIDEIO_LOG(INFO) << "ND2: dAspect is " << aspect << " in '" << fileStem
                << "'; it is read but not applied";
        }
    }
    else if (hasAcal) {
        const Variant* values = fieldOf(&acal, "CalibrationValues");
        const double x = number(values, "Calibration_11", 0);
        const double y = number(values, "Calibration_22", 0);
        info.pixelSizeX = x > 0 ? x * kMicro : 0;
        info.pixelSizeY = y > 0 ? y * kMicro : 0;
    }
    const double mag = hasVimd ? number(&vimd, "dObjectiveMag", 0) : 0;
    info.magnification = mag > 0 ? mag : 0;
    if (info.magnification <= 0 && hasVimd) {
        if (const Variant* name = fieldOf(&vimd, "wsObjectiveName")) {
            info.magnification = magnificationFromObjectiveName(name->asString());
        }
    }
    const double julian = hasVimd ? number(&vimd, "dTimeAbsolute", 0) : 0;
    info.absoluteJulianDay = julian >= 1 ? julian : 0;
    if (const Loop* z = info.experiment.loop(Axis::Z)) {
        info.zStep = zStepFrom(z->pars);
    }

    // The nominal step; each scene measures its own (design 6.3, R30).
    info.tStep = timeStepSeconds(info.experiment, {});
    return info;
}
