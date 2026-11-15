// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/mrxs/mrxsscene.hpp"
#include "slideio/drivers/mrxs/mrxsrenderer.hpp"
#include "slideio/drivers/mrxs/mrxsdescription.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/tools/tools.hpp"

#include <algorithm>
#include <cmath>

using namespace slideio;
using namespace slideio::mrxs;

namespace
{
    const int kLogicalTileSize = 256;
}

MRXSScene::MRXSScene(std::shared_ptr<const MRXSDataset> dataset) : m_dataset(std::move(dataset))
{
    // Slidedat.ini exactly as read; its structured form is buildMetadataTree's.
    m_rawMetadata = m_dataset->structure.ini.text();
    m_metadataFormat = MetadataFormat::Text;
    const auto& levels = m_dataset->geometry.levels;
    const double magnification = getMagnification();
    for (size_t l = 0; l < levels.size(); ++l) {
        const LevelGeometry& lg = levels[l];
        const cv::Rect box = levelBox(static_cast<int>(l));
        // The tile size is a logical read grid, not the irregular physical layout of the
        // drawables (design section 6).
        m_levels.emplace_back(static_cast<int>(l),
                              Size(box.width, box.height),
                              1.0 / double(lg.downsample), magnification / double(lg.downsample),
                              Size(kLogicalTileSize, kLogicalTileSize));
    }
}

std::string MRXSScene::getFilePath() const
{
    return m_dataset->structure.paths.entry.u8string();
}

const std::string& MRXSScene::getDriverId() const
{
    static const std::string id = "MRXS";
    return id;
}

std::string MRXSScene::getName() const
{
    return m_dataset->structure.paths.sceneName;
}

cv::Rect MRXSScene::getRect() const
{
    return cv::Rect(cv::Point(0, 0), levelBox(0).size());
}

cv::Rect MRXSScene::levelBox(int level) const
{
    if (m_dataset->crop) {
        return m_dataset->crop->levels[level];
    }
    const LevelGeometry& lg = m_dataset->geometry.levels[level];
    return cv::Rect(0, 0, static_cast<int>(lg.width), static_cast<int>(lg.height));
}

DriverOptions MRXSScene::getOpenOptions() const
{
    return {{"mrxs.crop_to_content", m_dataset->cropRequested ? "true" : "false"}};
}

DataType MRXSScene::getChannelDataType(int channel) const
{
    if (channel < 0 || channel >= getNumChannels()) {
        RAISE_RUNTIME_ERROR << "MRXS: invalid channel index " << channel;
    }
    return DataType::DT_Byte;
}

std::string MRXSScene::getChannelName(int channel) const
{
    static const char* names[] = {"Red", "Green", "Blue"};
    if (channel < 0 || channel >= getNumChannels()) {
        RAISE_RUNTIME_ERROR << "MRXS: invalid channel index " << channel;
    }
    return names[channel];
}

Resolution MRXSScene::getResolution() const
{
    // Level-0 micrometres per pixel, in metres; the INI already gives the stored level 0.
    const LevelMetadata& l0 = m_dataset->structure.metadata.levels[0];
    return Resolution(l0.mppX * 1e-6, l0.mppY * 1e-6);
}

double MRXSScene::getMagnification() const
{
    return m_dataset->structure.metadata.objective.value_or(0.0);
}

Compression MRXSScene::getCompression() const
{
    const auto& levels = m_dataset->structure.metadata.levels;
    const ImageFormat first = levels[0].format;
    for (const LevelMetadata& level : levels) {
        if (level.format != first) {
            return Compression::Unknown;   // mixed per-level codecs: no single answer
        }
    }
    switch (first) {
    case ImageFormat::Jpeg: return Compression::Jpeg;
    case ImageFormat::Png: return Compression::Png;
    case ImageFormat::Bmp24: return Compression::BMP;
    }
    return Compression::Unknown;
}

MetadataBuilder MRXSScene::buildMetadataTree() const
{
    return describe(*m_dataset);
}

double MRXSScene::getChannelBackground(int channel) const
{
    if (channel < 0 || channel >= getNumChannels()) {
        return CVScene::getChannelBackground(channel);
    }
    return m_dataset->background[channel];
}

void MRXSScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
                                             const std::vector<int>& channelIndices, int zSliceIndex,
                                             int tFrameIndex, cv::OutputArray output)
{
    const auto& levels = m_dataset->geometry.levels;
    auto selectLevel = [&levels](const cv::Rect& rect, const cv::Size& size) {
        if (rect.width <= 0 || rect.height <= 0) {
            return 0;
        }
        const double zoomX = double(size.width) / double(rect.width);
        const double zoomY = double(size.height) / double(rect.height);
        return Tools::findZoomLevel(std::max(zoomX, zoomY), static_cast<int>(levels.size()),
                                    [&levels](int index) { return 1.0 / double(levels[index].downsample); });
    };
    if (!m_dataset->crop) {
        const int level = selectLevel(blockRect, blockSize);
        const double scale = 1.0 / double(levels[level].downsample);
        cv::Rect levelRect;
        Tools::scaleRect(blockRect, scale, scale, levelRect);
        readResampledLevelBlockChannelsEx(level, levelRect, blockSize, channelIndices, zSliceIndex, tFrameIndex,
                                          output);
        return;
    }
    if (zSliceIndex != 0 || tFrameIndex != 0) {
        RAISE_RUNTIME_ERROR << "MRXS: the scene has one Z slice and one time frame";
    }
    validateChannels(channelIndices);
    // Cropped: clamp to the crop at level 0, then translate into nominal level-0 pixels
    // *before* scaling. Translating after scaling would not be exact: floor(C.x/d) +
    // ceil(W/d) is not always ceil((C.x + W)/d), so the read could cover a source column
    // more or fewer than the uncropped read of the same box (open options design 7.3).
    // The level read is clamped to the full level, not the level's box, for the same
    // reason; the level-0 clamp has already cut away everything outside the crop.
    const cv::Point origin = m_dataset->crop->level0.tl();
    readClampedBlock(getRect(), blockRect, blockSize, channelIndices,
        [&](const cv::Rect& validRect, const cv::Size& partSize, cv::OutputArray part) {
            const int level = selectLevel(validRect, partSize);
            const double scale = 1.0 / double(levels[level].downsample);
            cv::Rect levelRect;
            Tools::scaleRect(validRect + origin, scale, scale, levelRect);
            const LevelGeometry& lg = levels[level];
            readNominalLevel(level, levelRect,
                             cv::Rect(0, 0, static_cast<int>(lg.width), static_cast<int>(lg.height)),
                             partSize, channelIndices, part);
        },
        output);
}

void MRXSScene::readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect, const cv::Size& blockSize,
                                                  const std::vector<int>& channelIndices, int zSliceIndex,
                                                  int tFrameIndex, cv::OutputArray output)
{
    if (zSliceIndex != 0 || tFrameIndex != 0) {
        RAISE_RUNTIME_ERROR << "MRXS: the scene has one Z slice and one time frame";
    }
    validateLevel(level);
    validateChannels(channelIndices);
    // The level's box is the whole level when uncropped; cropped, the request is in the
    // box's coordinates and is translated into the nominal level and clamped to the box.
    const cv::Rect box = levelBox(level);
    readNominalLevel(level, levelRect + box.tl(), box, blockSize, channelIndices, output);
}

void MRXSScene::validateChannels(const std::vector<int>& channelIndices) const
{
    for (int channel : channelIndices) {
        if (channel < 0 || channel >= getNumChannels()) {
            RAISE_RUNTIME_ERROR << "MRXS: invalid channel index " << channel;
        }
    }
}

void MRXSScene::readNominalLevel(int level, const cv::Rect& levelRect, const cv::Rect& bounds,
                                 const cv::Size& blockSize, const std::vector<int>& channelIndices,
                                 cv::OutputArray output) const
{
    readClampedBlock(bounds, levelRect, blockSize, channelIndices,
        [&](const cv::Rect& validRect, const cv::Size& partSize, cv::OutputArray part) {
            const Renderer renderer(*m_dataset);
            const int64_t budget = m_dataset->runtime->options().maxResampleSourcePixels;
            cv::Mat rgb;
            if (partSize == validRect.size() || int64_t(validRect.width) * validRect.height <= budget) {
                renderer.render(level, validRect, rgb);
                if (rgb.size() != partSize) {
                    cv::Mat resized;
                    Tools::resize(rgb, resized, partSize);
                    rgb = resized;
                }
            }
            else {
                // Too large to render whole before resampling: render and resample it in
                // pieces of at most `budget` level pixels, each piece into its share of the
                // output, as TileComposer does for the tiled drivers.
                rgb.create(partSize, CV_8UC3);
                const double sx = double(validRect.width) / partSize.width;
                const double sy = double(validRect.height) / partSize.height;
                // A piece's source spans tw * sx pixels plus at most one for rounding.
                const double side = std::floor(std::sqrt(double(budget)));
                const int tw = std::max(1, static_cast<int>(std::floor((side - 1) / sx)));
                const int th = std::max(1, static_cast<int>(std::floor((side - 1) / sy)));
                for (int oy = 0; oy < partSize.height; oy += th) {
                    const int oh = std::min(th, partSize.height - oy);
                    const int y0 = static_cast<int>(std::floor(oy * sy));
                    const int y1 = std::min(validRect.height, static_cast<int>(std::ceil((oy + oh) * sy)));
                    for (int ox = 0; ox < partSize.width; ox += tw) {
                        const int ow = std::min(tw, partSize.width - ox);
                        const int x0 = static_cast<int>(std::floor(ox * sx));
                        const int x1 = std::min(validRect.width, static_cast<int>(std::ceil((ox + ow) * sx)));
                        cv::Mat piece;
                        renderer.render(level, cv::Rect(validRect.x + x0, validRect.y + y0, x1 - x0, y1 - y0), piece);
                        cv::Mat resized;
                        Tools::resize(piece, resized, cv::Size(ow, oh));
                        resized.copyTo(rgb(cv::Rect(ox, oy, ow, oh)));
                    }
                }
            }
            Tools::extractChannels(rgb, channelIndices, part);
        },
        output);
}
