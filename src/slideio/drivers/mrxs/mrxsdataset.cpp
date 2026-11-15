// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/mrxs/mrxsdataset.hpp"

#include <algorithm>
#include <cmath>

using namespace slideio::mrxs;

std::optional<CropBox> slideio::mrxs::computeCropBox(const Geometry& geometry)
{
    if (!geometry.bounds0 || geometry.levels.empty()) {
        return std::nullopt;
    }
    // Rounded outward exactly as mrxs.contentBounds is published, then clipped.
    const cv::Rect2d& b = *geometry.bounds0;
    const int64_t w0 = geometry.levels[0].width, h0 = geometry.levels[0].height;
    const int64_t x0 = std::max<int64_t>(0, int64_t(std::floor(b.x)));
    const int64_t y0 = std::max<int64_t>(0, int64_t(std::floor(b.y)));
    const int64_t x1 = std::min<int64_t>(w0, int64_t(std::ceil(b.x + b.width)));
    const int64_t y1 = std::min<int64_t>(h0, int64_t(std::ceil(b.y + b.height)));
    if (x1 <= x0 || y1 <= y0) {
        return std::nullopt;
    }
    CropBox box;
    box.level0 = cv::Rect(int(x0), int(y0), int(x1 - x0), int(y1 - y0));
    for (const LevelGeometry& level : geometry.levels) {
        const int64_t d = level.downsample;
        const int64_t lx0 = x0 / d, ly0 = y0 / d;      // non-negative: floor
        const int64_t lx1 = std::min(level.width, (x1 + d - 1) / d);
        const int64_t ly1 = std::min(level.height, (y1 + d - 1) / d);
        if (lx1 <= lx0 || ly1 <= ly0) {
            // A level floors the lattice, so content in the canvas's last few columns or
            // rows can fall outside it. This level and every coarser one would be empty:
            // the cropped pyramid ends at the previous level.
            break;
        }
        box.levels.emplace_back(int(lx0), int(ly0), int(lx1 - lx0), int(ly1 - ly0));
    }
    return box;
}

std::shared_ptr<const MRXSDataset> MRXSDataset::open(const std::string& entryPath, const RuntimeOptions& options,
                                                     bool cropToContent)
{
    auto dataset = std::make_shared<MRXSDataset>();
    dataset->structure = readSlideStructure(entryPath);
    dataset->geometry = Geometry::build(dataset->structure);
    std::vector<std::string> files;
    for (const auto& file : dataset->structure.metadata.dataFiles) {
        files.push_back(file.u8string());
    }
    dataset->runtime = std::make_shared<MRXSRuntime>(std::move(files), options);
    // IMAGE_FILL_COLOR_BGR: the low byte is red, then green, then blue.
    const int64_t fill = dataset->structure.metadata.levels[0].fillColorBGR;
    dataset->background = cv::Vec3b(static_cast<uchar>(fill & 0xFF), static_cast<uchar>((fill >> 8) & 0xFF),
                                     static_cast<uchar>((fill >> 16) & 0xFF));
    dataset->cropRequested = cropToContent;
    if (cropToContent) {
        dataset->crop = computeCropBox(dataset->geometry);
    }
    return dataset;
}
