// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/mrxs/mrxsdescription.hpp"
#include "slideio/core/metadata_internal.hpp"

#include <cmath>
#include <set>

using namespace slideio::mrxs;
using json = nlohmann::json;

namespace
{
    const char* formatName(ImageFormat format)
    {
        switch (format) {
        case ImageFormat::Jpeg: return "JPEG";
        case ImageFormat::Png: return "PNG";
        case ImageFormat::Bmp24: return "BMP24";
        }
        return "unknown";
    }

    const char* positionSourceName(PositionSource source)
    {
        switch (source) {
        case PositionSource::Raw: return "raw map";
        case PositionSource::Zlib: return "compressed map";
        case PositionSource::None: return "nominal positions";
        }
        return "unknown";
    }
}

slideio::MetadataBuilder slideio::mrxs::describe(const MRXSDataset& dataset)
{
    const SlideStructure& slide = dataset.structure;
    const Geometry& geometry = dataset.geometry;

    json ini = json::object();
    for (const IniFile::Group& group : slide.ini.groups()) {
        json& out = ini[group.name];
        out = json::object();
        for (const auto& entry : group.entries) {
            out[entry.first] = entry.second;
        }
    }

    json mrxs = json::object();
    mrxs["renderingProfile"] = "primary RGB pyramid; vendor masks, hardware-coordinate stitching and "
                               "separate filter channels are not applied";
    mrxs["positionSource"] = positionSourceName(slide.positionSource);
    mrxs["positionAware"] = geometry.positionAware;
    mrxs["nominalSize"] = {geometry.levels[0].width, geometry.levels[0].height};
    if (geometry.bounds0) {
        // As OpenSlide publishes them: rounded outward, not clipped to the canvas.
        const cv::Rect2d& b = *geometry.bounds0;
        const int64_t x0 = int64_t(std::floor(b.x)), y0 = int64_t(std::floor(b.y));
        const int64_t x1 = int64_t(std::ceil(b.x + b.width)), y1 = int64_t(std::ceil(b.y + b.height));
        mrxs["contentBounds"] = {x0, y0, x1 - x0, y1 - y0};
    }
    json levels = json::array();
    for (size_t l = 0; l < geometry.levels.size(); ++l) {
        levels.push_back({{"width", geometry.levels[l].width},
                          {"height", geometry.levels[l].height},
                          {"downsample", geometry.levels[l].downsample},
                          {"format", formatName(slide.metadata.levels[l].format)}});
    }
    mrxs["levels"] = levels;
    if (slide.metadata.objective) {
        mrxs["objectiveMagnification"] = *slide.metadata.objective;
    }
    // Everything the bundle declares that this driver does not render.
    json ignored = json::array();
    for (size_t i = 1; i < slide.metadata.hierarchical.size(); ++i) {
        ignored.push_back(slide.metadata.hierarchical[i].name);
    }
    static const std::set<std::string> consumed = {"VIMSLIDE_POSITION_BUFFER", "StitchingIntensityLayer",
                                                   "Scan data layer"};
    for (const Family& family : slide.metadata.nonHierarchical) {
        if (!consumed.count(family.name)) {
            ignored.push_back(family.name);
        }
    }
    mrxs["ignoredLayers"] = ignored;

    return detail::builderFromJson(json{{"ini", ini}, {"mrxs", mrxs}});
}
