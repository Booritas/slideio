// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/nd2/nd2imageinfo.hpp"
#include "slideio/drivers/nd2/nd2internal.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/log.hpp"
#include "slideio/core/tools/filereader.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>

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

    /// uLoopPars may arrive wrapped in an `i0000000000` map.
    const Variant& unwrap(const Variant& pars)
    {
        if (pars.kind() == Variant::Kind::Map && pars.get("i0000000000") && !pars.items().empty()) {
            return pars.items().front();
        }
        return pars;
    }

    /// Flag i of a validity list; true when there is no list.
    bool validAt(const Variant* flags, size_t i)
    {
        if (!flags) {
            return true;
        }
        if (flags->kind() == Variant::Kind::Bytes) {
            return i < flags->bytes().size() && flags->bytes()[i] != 0;
        }
        const std::vector<const Variant*> e = elementsOf(flags);
        if (e.empty()) {
            return i == 0 && (flags->asInt(0) != 0 || flags->asBool(false));
        }
        return i < e.size() && (e[i]->asInt(0) != 0 || e[i]->asBool(false));
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

    std::vector<double> buildFrameTimes(const ChunkFile& chunks, const FileReader& file, int64_t count)
    {
        std::vector<double> cache;
        const ChunkHeader* chunk = chunks.find("CustomData|AcqTimesCache!");
        if (chunk) {
            const std::vector<uint8_t> data = chunks.readData(file, *chunk);
            cache.resize(data.size() / sizeof(double));
            if (!cache.empty()) {
                std::memcpy(cache.data(), data.data(), cache.size() * sizeof(double));
            }
        }
        const auto& frames = chunks.frames();
        return mergeFrameTimes(cache, count, [&](int64_t frame, double& t) {
            const auto it = frames.find(frame);
            if (it == frames.end() || it->second.dataLength < sizeof(double)) {
                return false;
            }
            file.readAt(it->second.dataOffset(), &t, sizeof(t));
            return true;
        });
    }

    /// Seconds: `dPeriod` of the time loop; for an NE loop the first valid period's.
    double periodOf(const Loop& time)
    {
        double ms = 0;
        const Variant* periods = time.pars.get("pPeriod");
        if (periods) {
            const Variant* valid = time.pars.get("pPeriodValid");
            const std::vector<const Variant*> list = elementsOf(periods);
            for (size_t i = 0; i < list.size(); ++i) {
                if (validAt(valid, i)) {
                    const Variant* d = unwrap(*list[i]).get("dPeriod");
                    ms = d ? d->asDouble(0) : 0;
                    break;
                }
            }
        }
        else {
            const Variant* d = time.pars.get("dPeriod");
            ms = d ? d->asDouble(0) : 0;
        }
        return ms > 0 ? ms / 1000.0 : 0;
    }

    /// Mean difference of consecutive per-T times at p=0, z=0 in seconds; 0 unless all are known.
    double meanStepImpl(const Experiment& experiment, const std::vector<double>& times)
    {
        const int64_t n = experiment.count(Axis::T);
        if (static_cast<int64_t>(times.size()) < experiment.loopProduct()) {
            return 0;
        }
        if (n < 2) {
            return 0;
        }
        const double first = times[static_cast<size_t>(experiment.frameIndex(0, 0, 0))];
        double last = first;
        for (int64_t t = 1; t < n; ++t) {
            last = times[static_cast<size_t>(experiment.frameIndex(t, 0, 0))];
            if (std::isnan(first) || std::isnan(last)) {
                return 0;
            }
        }
        // Telescoping sum of the consecutive differences.
        return (last - first) / static_cast<double>(n - 1) / 1000.0;
    }
}

double slideio::nd2::zStepFrom(const Variant& pars)
{
    const Variant* step = pars.get("dZStep");
    double um = std::fabs(step ? step->asDouble(0) : 0);
    if (um == 0) {
        const Variant* count = pars.get("uiCount");
        const int64_t n = count ? count->asInt(0) : 0;
        if (n > 1) {
            const Variant* low = pars.get("dZLow");
            const Variant* high = pars.get("dZHigh");
            const double span = std::fabs((high ? high->asDouble(0) : 0) - (low ? low->asDouble(0) : 0));
            um = span / static_cast<double>(n - 1);
        }
    }
    return um * kMicro;
}

ImageInfo slideio::nd2::buildImageInfo(const ChunkFile& chunks, const FileReader& file,
    const std::string& fileStem)
{
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
    info.experiment = Experiment::parse(hasMeta ? &metaTree : nullptr);
    std::set<int64_t> present;
    for (const auto& frame : chunks.frames()) {
        present.insert(frame.first);
    }
    info.experiment.trimTime(present);
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

    info.frameTimesMs = buildFrameTimes(chunks, file, info.experiment.loopProduct());
    if (const Loop* t = info.experiment.loop(Axis::T)) {
        info.tStep = periodOf(*t);
        if (info.tStep <= 0) {
            info.tStep = meanStepSeconds(info.experiment, info.frameTimesMs);
        }
    }
    return info;
}

std::vector<double> slideio::nd2::mergeFrameTimes(const std::vector<double>& cache, int64_t count,
    const std::function<bool(int64_t, double&)>& prefixTime)
{
    std::vector<double> times(static_cast<size_t>(std::max<int64_t>(count, 0)), kNaN);
    const size_t covered = std::min(cache.size(), times.size());
    std::copy(cache.begin(), cache.begin() + covered, times.begin());
    for (size_t i = covered; i < times.size(); ++i) {
        double t = 0;
        if (prefixTime(static_cast<int64_t>(i), t)) {
            times[i] = t;
        }
    }
    return times;
}

double slideio::nd2::meanStepSeconds(const Experiment& experiment, const std::vector<double>& times)
{
    return meanStepImpl(experiment, times);
}
