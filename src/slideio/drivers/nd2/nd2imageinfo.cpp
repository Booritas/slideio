// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/nd2/nd2imageinfo.hpp"
#include "slideio/drivers/nd2/nd2internal.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/log.hpp"
#include "slideio/core/tools/endian.hpp"
#include "slideio/core/tools/filereader.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <sstream>
#include <locale>
#include <map>
#include <memory>

using namespace slideio;
using namespace slideio::nd2;
using slideio::nd2::detail::elementsOf;

namespace
{
    constexpr double kMicro = 1e-6;
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

    /// Decodes the chunk `<base>[LV]<suffix>!`. Version 2 tries the XML name first, then
    /// the LV one; version 3 only the LV one. `key` is the name without LV and '!'.
    bool loadTree(const ChunkFile& chunks, const FileReader& file, const std::string& base,
        const std::string& suffix, Variant& out, std::string& key)
    {
        std::vector<std::string> names;
        if (chunks.versionMajor() < 3) {
            names.push_back(base + suffix + "!");
        }
        names.push_back(base + "LV" + suffix + "!");
        for (const std::string& name : names) {
            const ChunkHeader* h = chunks.find(name);
            if (h) {
                out = decodeMetadata(chunks.readData(file, *h));
                key = base + suffix;
                return true;
            }
        }
        return false;
    }

    const Variant* fieldOf(const Variant* primary, const Variant* secondary, const char* field)
    {
        const Variant* v = primary ? primary->get(field) : nullptr;
        if (!v && secondary) {
            v = secondary->get(field);
        }
        return v;
    }

    double number(const Variant* primary, const Variant* secondary, const char* field, double fallback)
    {
        const Variant* v = fieldOf(primary, secondary, field);
        return v ? v->asDouble(fallback) : fallback;
    }

    std::vector<Position> buildPositions(const Experiment& experiment, const std::string& fileStem)
    {
        std::vector<Position> out;
        const Loop* xy = experiment.loop(Axis::P);
        if (!xy) {
            out.push_back(Position{ fileStem, std::string() });
            return out;
        }
        const std::vector<const Variant*> records = elementsOf(xy->pars.get("Points"));
        std::set<std::string> used;
        for (size_t k = 0; k < xy->validItems.size(); ++k) {
            const int64_t item = xy->validItems[k];
            const Variant* record = (item >= 0 && static_cast<size_t>(item) < records.size())
                ? records[static_cast<size_t>(item)] : nullptr;
            Position p;
            if (record) {
                p.rawJson = record->toJson().dump();
                const Variant* n = record->get("dPosName");
                if (n) {
                    p.name = n->asString();
                }
            }
            if (p.name.empty()) {
                p.name = "Position " + std::to_string(k + 1);
            }
            if (used.count(p.name)) {
                p.name += " (" + std::to_string(k + 1) + ")";
            }
            used.insert(p.name);
            out.push_back(std::move(p));
        }
        return out;
    }

    /// Design 6.4: the AcqTimesCache table, then each frame's 8-byte prefix, read when
    /// first asked for (PR review: not one positional read per frame at open).
    FrameTimes buildFrameTimes(const ChunkFile& chunks, const std::shared_ptr<const FileReader>& file,
        int64_t count)
    {
        std::vector<double> cache;
        const ChunkHeader* chunk = chunks.find("CustomData|AcqTimesCache!");
        if (chunk) {
            const std::vector<uint8_t> data = chunks.readData(*file, *chunk);
            cache.resize(data.size() / sizeof(double));
            if (!cache.empty()) {
                std::memcpy(cache.data(), data.data(), cache.size() * sizeof(double));
                Endian::fromLittleEndianToNative(cache.data(), cache.size());
            }
        }
        auto frames = std::make_shared<const std::map<int64_t, ChunkHeader>>(chunks.frames());
        return FrameTimes(std::move(cache), count, [file, frames](int64_t frame) {
            const auto it = frames->find(frame);
            if (it == frames->end() || it->second.dataLength < sizeof(double)) {
                return kNaN;
            }
            double t = 0;
            file->readAt(it->second.dataOffset(), &t, sizeof(t));
            return Endian::fromLittleEndianToNative(t);
        });
    }

    /// Mean difference of consecutive per-T times at p=0, z=0 in seconds; 0 unless all are known.
    double meanStepImpl(const Experiment& experiment, const FrameTimes& times, int64_t p)
    {
        const int64_t n = experiment.count(Axis::T);
        if (times.size() < experiment.loopProduct()) {
            return 0;
        }
        if (n < 2) {
            return 0;
        }
        if (p < 0 || p >= experiment.count(Axis::P)) {
            return 0;
        }
        const double first = times[experiment.frameIndex(0, p, 0)];
        double last = first;
        for (int64_t t = 1; t < n; ++t) {
            last = times[experiment.frameIndex(t, p, 0)];
            if (std::isnan(first) || std::isnan(last)) {
                return 0;
            }
        }
        // Telescoping sum of the consecutive differences.
        return (last - first) / static_cast<double>(n - 1) / 1000.0;
    }
}

double slideio::nd2::detail::periodOf(const Loop& time)
{
    double ms = 0;
    const Variant* periods = detail::fieldOf(&time.pars, "pPeriod");
    if (periods && (periods->kind() == Variant::Kind::Map || periods->kind() == Variant::Kind::List)) {
        bool hasFlags = false;
        const std::vector<bool> valid = detail::flagsOf(detail::fieldOf(&time.pars, "pPeriodValid"), hasFlags);
        const std::vector<const Variant*> list = elementsOf(periods);
        for (size_t i = 0; i < list.size(); ++i) {
            if (detail::flagSet(valid, hasFlags, i)) {
                const Variant* d = detail::fieldOf(&detail::unwrapPars(*list[i]), "dPeriod");
                ms = d ? d->asDouble(0) : 0;
                break;
            }
        }
    }
    else {
        const Variant* d = detail::fieldOf(&time.pars, "dPeriod");
        ms = d ? d->asDouble(0) : 0;
    }
    return ms > 0 ? ms / 1000.0 : 0;
}

double slideio::nd2::zStepFrom(const Variant& pars)
{
    const Variant* step = detail::fieldOf(&pars, "dZStep");
    double um = std::fabs(step ? step->asDouble(0) : 0);
    if (um == 0) {
        const Variant* count = detail::fieldOf(&pars, "uiCount");
        const int64_t n = count ? count->asInt(0) : 0;
        if (n > 1) {
            const Variant* low = detail::fieldOf(&pars, "dZLow");
            const Variant* high = detail::fieldOf(&pars, "dZHigh");
            const double span = std::fabs((high ? high->asDouble(0) : 0) - (low ? low->asDouble(0) : 0));
            um = span / static_cast<double>(n - 1);
        }
    }
    return um * kMicro;
}

ImageInfo slideio::nd2::buildImageInfo(const ChunkFile& chunks, std::shared_ptr<const FileReader> fileRef,
    const std::string& fileStem)
{
    const FileReader& file = *fileRef;
    ImageInfo info;
    nlohmann::json raw = nlohmann::json::object();
    raw["version"] = std::to_string(chunks.versionMajor()) + "." + std::to_string(chunks.versionMinor());

    Variant attrTree, metaTree, pictureTree, textTree, calibTree;
    std::string key;
    if (!loadTree(chunks, file, "ImageAttributes", "", attrTree, key)) {
        RAISE_RUNTIME_ERROR << "ND2: the file has no ImageAttributes chunk";
    }
    raw[key] = attrTree.toJson();
    const bool hasMeta = loadTree(chunks, file, "ImageMetadata", "", metaTree, key);
    if (hasMeta) {
        raw[key] = metaTree.toJson();
    }
    const bool hasPicture = loadTree(chunks, file, "ImageMetadataSeq", "|0", pictureTree, key);
    if (hasPicture) {
        raw[key] = pictureTree.toJson();
    }
    if (loadTree(chunks, file, "ImageTextInfo", "", textTree, key)) {
        raw[key] = textTree.toJson();
    }
    const bool hasCalib = loadTree(chunks, file, "ImageCalibration", "|0", calibTree, key);
    if (hasCalib) {
        raw[key] = calibTree.toJson();
    }
    info.rawJson = raw.dump();

    info.attributes = Attributes::parse(attrTree);
    info.channels = parseChannels(hasPicture ? &pictureTree : nullptr, info.attributes.components);
    std::set<int64_t> present;
    for (const auto& frame : chunks.frames()) {
        present.insert(frame.first);
    }
    // Design 4.5: the loops may declare at most maxDeclaredFrames(present) frames, which
    // bounds everything below that is sized by them.
    info.experiment = Experiment::parse(hasMeta ? &metaTree : nullptr,
        maxDeclaredFrames(static_cast<int64_t>(present.size())));
    // Frame times decide whether surplus time points continue the series (R29); they are
    // read as far as a fitted T could reach (R31), then cut to the fitted product.
    const int64_t reach = info.experiment.loopProduct() + static_cast<int64_t>(present.size());
    FrameTimes times = buildFrameTimes(chunks, fileRef, reach);
    info.experiment.fitTime(present, times);
    times.resize(info.experiment.loopProduct());
    info.positions = buildPositions(info.experiment, fileStem);

    // Design 6.3. The fields live in the picture metadata; the calibration chunk is the fallback.
    const Variant* pic = hasPicture ? &pictureTree : nullptr;
    const Variant* cal = hasCalib ? &calibTree : nullptr;
    const Variant* calibrated = fieldOf(pic, cal, "bCalibrated");
    const double um = number(pic, cal, "dCalibration", 0);
    if (calibrated && (calibrated->asBool(false) || calibrated->asInt(0) != 0) && um > 0) {
        info.pixelSizeX = um * kMicro;
        info.pixelSizeY = um * kMicro;
    }
    const double mag = number(pic, cal, "dObjectiveMag", 0);
    info.magnification = mag > 0 ? mag : 0;
    if (info.magnification <= 0) {
        if (const Variant* name = fieldOf(pic, cal, "wsObjectiveName")) {
            info.magnification = magnificationFromObjectiveName(name->asString());
        }
    }
    const double julian = number(pic, cal, "dTimeAbsolute", 0);
    info.absoluteJulianDay = julian >= 1 ? julian : 0;
    const double aspect = number(pic, cal, "dAspect", 1);
    if (aspect != 1) {
        SLIDEIO_LOG(INFO) << "ND2: dAspect is " << aspect << " in '" << fileStem
            << "'; it is read but not applied";
    }

    if (const Loop* z = info.experiment.loop(Axis::Z)) {
        info.zStep = zStepFrom(z->pars);
    }

    info.frameTimesMs = std::move(times);
    // The nominal step; each scene measures its own (design 6.3, R30).
    info.tStep = timeStepSeconds(info.experiment, {});
    return info;
}

double slideio::nd2::magnificationFromObjectiveName(const std::string& name)
{
    const auto isDigit = [](char c) { return c >= '0' && c <= '9'; };
    const auto isAsciiAlnum = [&](char c) {
        return isDigit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    };
    for (size_t i = 1; i < name.size(); ++i) {
        if ((name[i] != 'x' && name[i] != 'X') || !isDigit(name[i - 1])) {
            continue;
        }
        size_t start = i;
        while (start > 0 && (isDigit(name[start - 1]) || name[start - 1] == '.')) {
            --start;
        }
        // The number must begin a word: "N2x" or "A1.4x" is not a magnification.
        if (start > 0 && isAsciiAlnum(name[start - 1])) {
            continue;
        }
        std::istringstream in(name.substr(start, i - start));
        in.imbue(std::locale::classic());
        double value = 0;
        if (in >> value && in.eof() && value > 0) {
            return value;
        }
    }
    return 0;
}

double slideio::nd2::timeStepSeconds(const Experiment& experiment, const FrameTimes& times, int64_t p)
{
    const Loop* t = experiment.loop(Axis::T);
    if (!t) {
        return 0;
    }
    const double measured = meanStepImpl(experiment, times, p);
    return measured > 0 ? measured : std::max(0., detail::periodOf(*t));
}

double slideio::nd2::meanStepSeconds(const Experiment& experiment, const FrameTimes& times, int64_t p)
{
    return meanStepImpl(experiment, times, p);
}
