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
using slideio::nd2::detail::elementsOf;

namespace
{
    const char kMapSignature[] = "LABORATORY IMAGING ND BOX MAP 00";
    constexpr uint64_t kSignatureSize = 32;
    constexpr uint64_t kTrailerSize = 40;      // signature + u64 distance from EOF to the map
    constexpr uint64_t kMapEntrySize = 16;     // JP2 type, LIM type, u64 LE offset
    constexpr uint64_t kIhdrSize = 14;
    constexpr double kMicro = 1e-6;
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    /// The loop tree is a chain in practice; a deeper one is a damaged file.
    constexpr int kMaxDepth = 64;
    /// A VIMD box keeps its time near the start; this much is read before the whole box.
    constexpr uint64_t kTimeProbeBytes = 4096;

    constexpr int kEtTime = 1;
    constexpr int kEtXY = 2;
    constexpr int kEtZ = 4;
    constexpr int kEtSpectral = 6;
    constexpr int kEtNeTime = 8;

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

    /// The legacy writers prefix names with their type (`uiCount`, `dZStep`, `eType`);
    /// the nd2 package strips `^[a-z_]+`, and the fixtures use the stripped names.
    std::string stripped(const std::string& name)
    {
        size_t i = 0;
        while (i < name.size() && ((name[i] >= 'a' && name[i] <= 'z') || name[i] == '_')) {
            ++i;
        }
        return i == name.size() ? name : name.substr(i);
    }

    /// Member `name` of a Map, matched as written or after stripping the type prefix.
    const Variant* field(const Variant* v, const std::string& name)
    {
        if (!v) {
            return nullptr;
        }
        if (const Variant* exact = v->get(name)) {
            return exact;
        }
        for (const auto& m : v->members()) {
            if (stripped(m.first) == name) {
                return &m.second;
            }
        }
        return nullptr;
    }

    double number(const Variant* v, const std::string& name, double fallback)
    {
        const Variant* f = field(v, name);
        return f ? f->asDouble(fallback) : fallback;
    }

    /// A single-element wrapper (a `no_name` list of one, an `i0000000000` map) around a record.
    const Variant& single(const Variant& v)
    {
        const Variant* node = &v;
        for (int i = 0; i < kMaxDepth && node->items().size() == 1
            && (node->kind() == Variant::Kind::List || node->members().size() == 1); ++i) {
            node = &node->items().front();
        }
        return *node;
    }

    /// Flags of a validity list; `present` is false when there is none or it is empty.
    std::vector<bool> flagsOf(const Variant* v, bool& present)
    {
        std::vector<bool> flags;
        if (v) {
            if (v->kind() == Variant::Kind::Bytes) {
                for (uint8_t b : v->bytes()) {
                    flags.push_back(b != 0);
                }
            }
            else if (v->kind() == Variant::Kind::Map || v->kind() == Variant::Kind::List) {
                for (const Variant* e : elementsOf(v)) {
                    flags.push_back(e->asBool(false));
                }
            }
            else if (v->kind() != Variant::Kind::Null) {
                flags.push_back(v->asBool(false));
            }
        }
        present = !flags.empty();
        return flags;
    }

    int64_t countOf(const Variant* pars)
    {
        const Variant* c = field(pars, "Count");
        return c ? c->asInt(0) : 0;
    }

    struct RawLoop
    {
        int type = 0;
        int level = 0;
        int64_t count = 0;
        Variant pars;
        std::vector<int64_t> validItems;
    };

    /// NE time (FD 7.1): the counts of the valid periods; `Count` when there is no period list.
    int64_t neTimeCount(const Variant& pars)
    {
        const std::vector<const Variant*> periods = elementsOf(field(&pars, "Period"));
        if (periods.empty()) {
            return countOf(&pars);
        }
        bool hasFlags = false;
        const std::vector<bool> valid = flagsOf(field(&pars, "PeriodValid"), hasFlags);
        int64_t total = 0;
        for (size_t i = 0; i < periods.size(); ++i) {
            if (!hasFlags || (i < valid.size() && valid[i])) {
                const int64_t n = countOf(&single(*periods[i]));
                if (n > 0 && total <= std::numeric_limits<int64_t>::max() - n) {
                    total += n;
                }
            }
        }
        return total;
    }

    /// XY: the points of `PosX` (or `Count`) whose `ItemValid` flag is set; all without flags.
    void countPoints(const Variant& node, RawLoop& loop)
    {
        int64_t n = static_cast<int64_t>(elementsOf(field(&loop.pars, "PosX")).size());
        if (n == 0) {
            n = countOf(&loop.pars);
        }
        bool hasFlags = false;
        const std::vector<bool> valid = flagsOf(field(&node, "ItemValid"), hasFlags);
        for (int64_t i = 0; i < n; ++i) {
            if (!hasFlags || (static_cast<size_t>(i) < valid.size() && valid[static_cast<size_t>(i)])) {
                loop.validItems.push_back(i);
            }
        }
        loop.count = static_cast<int64_t>(loop.validItems.size());
    }

    /// FD 7.2 rules over the legacy names (FD 11.2): a spectral loop is skipped and its
    /// children stand at its level.
    void flatten(const Variant& node, int level, int depth, std::vector<RawLoop>& out)
    {
        if (depth > kMaxDepth) {
            throw Unsupported("ND2: unsupported experiment: loop tree deeper than " + std::to_string(kMaxDepth));
        }
        const Variant* typeField = field(&node, "Type");
        const int type = typeField ? static_cast<int>(typeField->asInt(0)) : 0;
        const Variant* parsField = field(&node, "LoopPars");
        int childLevel = level + 1;
        if (type == kEtSpectral) {
            childLevel = level;
        }
        else if (parsField) {
            RawLoop loop;
            loop.type = type;
            loop.level = level;
            loop.pars = single(*parsField);
            if (type == kEtXY) {
                countPoints(node, loop);
            }
            else if (type == kEtNeTime) {
                loop.count = neTimeCount(loop.pars);
            }
            else {
                loop.count = countOf(&loop.pars);
            }
            if (loop.count > 0) {
                if (out.empty() || out.back().level < level) {
                    out.push_back(std::move(loop));
                }
                else if (out.back().level == level && out.back().type == type && out.back().count < loop.count) {
                    out.back() = std::move(loop);
                }
            }
        }
        const Variant* next = field(&node, "NextLevelEx");
        if (!next) {
            return;
        }
        if (field(next, "Type")) {
            flatten(*next, childLevel, depth + 1, out);   // the child record itself
            return;
        }
        for (const Variant* child : elementsOf(next)) {
            flatten(single(*child), childLevel, depth + 1, out);
        }
    }

    /// AIM1/AIMD: the loop record under its wrappers (`vMetadata`, `no_name`), or the old
    /// `LoopNo00`, `LoopNo01`... list, one loop per level.
    std::vector<RawLoop> readLoops(const Variant& aim)
    {
        const Variant* node = &aim;
        for (int i = 0; i < kMaxDepth && !field(node, "Type"); ++i) {
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
        std::vector<RawLoop> raw;
        if (node->get("LoopNo00")) {
            int level = 0;
            for (const auto& m : node->members()) {
                if (m.first.compare(0, 6, "LoopNo") == 0) {
                    flatten(single(m.second), level++, 0, raw);
                }
            }
        }
        else if (field(node, "Type")) {
            flatten(*node, 0, 0, raw);
        }
        return raw;
    }

    std::string describe(const std::vector<RawLoop>& loops)
    {
        std::string s;
        for (const RawLoop& l : loops) {
            if (!s.empty()) {
                s += ", ";
            }
            s += "type " + std::to_string(l.type) + ":" + std::to_string(l.count);
        }
        return s;
    }

    /// The text of the TINF item that starts with "Metadata:"; empty when there is none.
    std::string metadataText(const Variant& v, int depth = 0)
    {
        if (depth > kMaxDepth) {
            return std::string();
        }
        for (const auto& m : v.members()) {
            if (m.second.kind() == Variant::Kind::String && m.first == "Text"
                && m.second.asString().compare(0, 9, "Metadata:") == 0) {
                return m.second.asString();
            }
            const std::string found = metadataText(m.second, depth + 1);
            if (!found.empty()) {
                return found;
            }
        }
        for (const Variant& item : v.items()) {
            const std::string found = metadataText(item, depth + 1);
            if (!found.empty()) {
                return found;
            }
        }
        return std::string();
    }

    Experiment buildExperiment(const Variant* aim, int64_t textXY, const std::string& fileStem)
    {
        std::vector<RawLoop> raw;
        if (aim) {
            raw = readLoops(*aim);
        }
        std::vector<Loop> loops;
        for (RawLoop& l : raw) {
            Loop loop;
            switch (l.type) {
            case kEtTime: case kEtNeTime: loop.axis = Axis::T; break;
            case kEtXY: loop.axis = Axis::P; break;
            case kEtZ: loop.axis = Axis::Z; break;
            default:
                if (l.count > 1) {
                    throw Unsupported("ND2: unsupported experiment (loop type " + std::to_string(l.type)
                        + " is not supported): " + describe(raw));
                }
                continue;   // one item: no dimension
            }
            loop.count = l.count;
            loop.pars = std::move(l.pars);
            loop.validItems = std::move(l.validItems);
            if (loop.axis == Axis::P && textXY > 0 && textXY != loop.count) {
                // FD 11.3: the Dimensions text rules over the loop for the XY count.
                SLIDEIO_LOG(INFO) << "ND2: '" << fileStem << "': the XY loop has " << loop.count
                    << " valid points; the text info says XY(" << textXY << "), which is used";
                loop.count = textXY;
                loop.validItems.clear();
                for (int64_t i = 0; i < textXY; ++i) {
                    loop.validItems.push_back(i);
                }
            }
            loops.push_back(std::move(loop));
        }
        return Experiment::fromLoops(std::move(loops));
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

    std::vector<ChannelInfo> buildChannels(const Variant* planes, int64_t channelCount, int perChannel)
    {
        const Variant* records = field(planes, "PlaneNew");
        if (!records) {
            records = field(planes, "Plane");
        }
        const std::vector<const Variant*> plane = elementsOf(records);
        static const char* const rgb[] = { "R", "G", "B" };
        std::vector<ChannelInfo> out;
        for (int c = 0; c < channelCount; ++c) {
            std::string base;
            uint32_t color = 0xFFFFFF;
            std::string probe;
            if (static_cast<size_t>(c) < plane.size()) {
                const Variant* rec = &single(*plane[static_cast<size_t>(c)]);
                if (const Variant* d = field(rec, "Description")) {
                    base = d->asString();
                }
                if (base.empty()) {
                    if (const Variant* n = field(rec, "OpticalConfigName")) {
                        base = n->asString();
                    }
                }
                if (const Variant* col = field(rec, "Color")) {
                    color = static_cast<uint32_t>(col->asInt(0xFFFFFF));
                }
                if (const Variant* p = field(field(rec, "FluorescentProbe"), "Name")) {
                    probe = p->asString();
                }
            }
            for (int j = 0; j < perChannel; ++j) {
                ChannelInfo ch;
                ch.nd2Channel = c;
                ch.component = j;
                ch.color = color;
                if (perChannel == 1) {
                    ch.name = base;
                }
                else {
                    const std::string suffix = perChannel == 3 ? rgb[j] : std::to_string(j);
                    ch.name = base.empty() ? suffix : base + " " + suffix;
                }
                if (!probe.empty()) {
                    ch.attributes.emplace_back("FluorescentProbe", probe);
                }
                out.push_back(std::move(ch));
            }
        }
        return out;
    }

    /// Seconds between time points: the time loop's period (the first valid one of an NE loop).
    double periodSeconds(const Loop& time)
    {
        double ms = 0;
        const Variant* periods = field(&time.pars, "Period");
        const std::vector<const Variant*> list = elementsOf(periods);
        if (!list.empty()) {
            bool hasFlags = false;
            const std::vector<bool> valid = flagsOf(field(&time.pars, "PeriodValid"), hasFlags);
            for (size_t i = 0; i < list.size(); ++i) {
                if (!hasFlags || (i < valid.size() && valid[i])) {
                    ms = number(&single(*list[i]), "Period", 0);
                    break;
                }
            }
        }
        else if (periods) {
            ms = periods->asDouble(0);
        }
        return ms > 0 ? ms / 1000.0 : 0;
    }

    double zStepMetres(const Loop& z)
    {
        double um = std::fabs(number(&z.pars, "ZStep", 0));
        const int64_t n = countOf(&z.pars);
        if (um == 0 && n > 1) {
            um = std::fabs(number(&z.pars, "ZHigh", 0) - number(&z.pars, "ZLow", 0)) / static_cast<double>(n - 1);
        }
        return um * kMicro;
    }

    std::vector<Position> buildPositions(const Experiment& experiment, const std::string& fileStem)
    {
        std::vector<Position> out;
        const Loop* xy = experiment.loop(Axis::P);
        if (!xy) {
            out.push_back(Position{ fileStem, std::string() });
            return out;
        }
        const char* const axes[] = { "PosX", "PosY", "PosZ", "PFSOffset" };
        for (size_t k = 0; k < xy->validItems.size(); ++k) {
            const size_t item = static_cast<size_t>(xy->validItems[k]);
            Position p;
            p.name = "Position " + std::to_string(k + 1);
            nlohmann::json point = nlohmann::json::object();
            for (const char* axis : axes) {
                const std::vector<const Variant*> values = elementsOf(field(&xy->pars, axis));
                if (item < values.size()) {
                    point[axis] = values[item]->toJson();
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

std::vector<uint8_t> LegacyFile::readBox(const FileReader& file, const Box& box) const
{
    if (box.dataLength > std::numeric_limits<size_t>::max()) {
        damaged("legacy box at offset " + std::to_string(box.offset) + " is too large to read");
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
    if (legacy.signedSamples() || (legacy.bits() != 8 && legacy.bits() != 16)) {
        throw Unsupported("ND2: unsupported legacy sample type: ihdr gives " + std::to_string(legacy.bits())
            + "-bit " + (legacy.signedSamples() ? "signed" : "unsigned") + " samples (expected 8 or 16 bit unsigned)");
    }
    // Channels from VIMD.PicturePlanes.Count of frame 0, never from the spectral loop.
    const Variant* planes = hasVimd ? field(&vimd, "PicturePlanes") : nullptr;
    int64_t channelCount = countOf(planes);
    if (channelCount <= 0) {
        const Variant* records = field(planes, "PlaneNew");
        channelCount = static_cast<int64_t>(elementsOf(records ? records : field(planes, "Plane")).size());
    }
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
    a.bitsInMemory = legacy.bits();
    const Variant* significantField = hasArtt ? field(&artt, "SignificantBits") : nullptr;
    const int64_t significant = significantField ? significantField->asInt(0) : 0;
    a.significantBits = (significant > 0 && significant <= a.bitsInMemory) ? static_cast<int>(significant) : a.bitsInMemory;
    a.rowStride = static_cast<int64_t>(a.width) * a.components * a.sampleBytes();
    a.compression = FrameCompression::None;
    a.sequenceCount = static_cast<int64_t>(legacy.all("VCAL").size());
    info.channels = buildChannels(planes, channelCount, perChannel);

    // Dimensions (FD 11.3): the AIM loops, with the XY count from the TINF text.
    const int64_t textXY = hasTinf ? xyCountFromText(metadataText(tinf)) : -1;
    info.experiment = buildExperiment(hasAim ? &aim : nullptr, textXY, fileStem);
    const int64_t streams = static_cast<int64_t>(legacy.codestreams().size());
    const int64_t framesInFile = (streams + channelCount - 1) / channelCount;
    std::set<int64_t> present;
    for (int64_t f = 0; f < framesInFile && f < info.experiment.loopProduct(); ++f) {
        present.insert(f);
    }
    info.experiment.trimTime(present);
    info.positions = buildPositions(info.experiment, fileStem);

    // Calibration (design 6.3): VCAL of frame 0, else ACAL; 0 when uncalibrated or <= 0.
    if (hasVcal) {
        const Variant* flag = field(&vcal, "Calibrated");
        if (!flag) {
            flag = field(&vcal, "Valid");
        }
        const double um = number(&vcal, "Calibration", 0);
        if (flag && flag->asBool(false) && um > 0) {
            info.pixelSizeX = um * kMicro;
            info.pixelSizeY = um * kMicro;
        }
        const double aspect = number(&vcal, "Aspect", 1);
        if (aspect != 1) {
            SLIDEIO_LOG(INFO) << "ND2: dAspect is " << aspect << " in '" << fileStem
                << "'; it is read but not applied";
        }
    }
    else if (hasAcal) {
        const Variant* values = field(&acal, "CalibrationValues");
        const double x = number(values, "Calibration_11", 0);
        const double y = number(values, "Calibration_22", 0);
        info.pixelSizeX = x > 0 ? x * kMicro : 0;
        info.pixelSizeY = y > 0 ? y * kMicro : 0;
    }
    const double mag = hasVimd ? number(&vimd, "ObjectiveMag", 0) : 0;
    info.magnification = mag > 0 ? mag : 0;
    const double julian = hasVimd ? number(&vimd, "TimeAbsolute", 0) : 0;
    info.absoluteJulianDay = julian >= 1 ? julian : 0;
    if (const Loop* z = info.experiment.loop(Axis::Z)) {
        info.zStep = zStepMetres(*z);
    }

    // Frame times: TimeMSec of each frame's VIMD, else NaN.
    const std::vector<const Box*> vimds = legacy.all("VIMD");
    const int64_t product = info.experiment.loopProduct();
    info.frameTimesMs.assign(static_cast<size_t>(product), kNaN);
    for (int64_t f = 0; f < product && f < static_cast<int64_t>(vimds.size()); ++f) {
        info.frameTimesMs[static_cast<size_t>(f)] = f == 0
            ? number(&vimd, "TimeMSec", kNaN) : frameTime(legacy, file, *vimds[static_cast<size_t>(f)]);
    }
    if (const Loop* t = info.experiment.loop(Axis::T)) {
        info.tStep = periodSeconds(*t);
        if (info.tStep <= 0) {
            info.tStep = meanStepSeconds(info.experiment, info.frameTimesMs);
        }
    }
    return info;
}
