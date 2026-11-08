// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/mrxs/mrxsdataset.hpp"

using namespace slideio::mrxs;

std::shared_ptr<const MRXSDataset> MRXSDataset::open(const std::string& entryPath, const RuntimeOptions& options)
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
    return dataset;
}
