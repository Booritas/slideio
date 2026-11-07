// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/mrxs/mrxsscene.hpp"
#include "slideio/drivers/mrxs/mrxsrenderer.hpp"
#include "slideio/drivers/mrxs/mrxsdescription.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/tools/tools.hpp"

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
        // The tile size is a logical read grid, not the irregular physical layout of the
        // drawables (design section 6).
        m_levels.emplace_back(static_cast<int>(l),
                              Size(static_cast<int>(lg.width), static_cast<int>(lg.height)),
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
    const LevelGeometry& l0 = m_dataset->geometry.levels[0];
    return cv::Rect(0, 0, static_cast<int>(l0.width), static_cast<int>(l0.height));
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
    int level = 0;
    if (blockRect.width > 0 && blockRect.height > 0) {
        const double zoomX = double(blockSize.width) / double(blockRect.width);
        const double zoomY = double(blockSize.height) / double(blockRect.height);
        level = Tools::findZoomLevel(std::max(zoomX, zoomY), static_cast<int>(levels.size()),
                                     [&levels](int index) { return 1.0 / double(levels[index].downsample); });
    }
    const double scale = 1.0 / double(levels[level].downsample);
    cv::Rect levelRect;
    Tools::scaleRect(blockRect, scale, scale, levelRect);
    readResampledLevelBlockChannelsEx(level, levelRect, blockSize, channelIndices, zSliceIndex, tFrameIndex, output);
}

void MRXSScene::readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect, const cv::Size& blockSize,
                                                  const std::vector<int>& channelIndices, int zSliceIndex,
                                                  int tFrameIndex, cv::OutputArray output)
{
    if (zSliceIndex != 0 || tFrameIndex != 0) {
        RAISE_RUNTIME_ERROR << "MRXS: the scene has one Z slice and one time frame";
    }
    validateLevel(level);
    for (int channel : channelIndices) {
        if (channel < 0 || channel >= getNumChannels()) {
            RAISE_RUNTIME_ERROR << "MRXS: invalid channel index " << channel;
        }
    }
    const LevelGeometry& lg = m_dataset->geometry.levels[level];
    const cv::Rect bounds(0, 0, static_cast<int>(lg.width), static_cast<int>(lg.height));
    readClampedBlock(bounds, levelRect, blockSize, channelIndices,
        [&](const cv::Rect& validRect, const cv::Size& partSize, cv::OutputArray part) {
            cv::Mat rgb;
            Renderer(*m_dataset).render(level, validRect, rgb);
            if (rgb.size() != partSize) {
                cv::Mat resized;
                Tools::resize(rgb, resized, partSize);
                rgb = resized;
            }
            Tools::extractChannels(rgb, channelIndices, part);
        },
        output);
}
