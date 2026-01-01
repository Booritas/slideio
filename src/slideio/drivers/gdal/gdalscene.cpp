// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/base/exceptions.hpp"
#include "slideio/drivers/gdal/gdalscene.hpp"

#include <opencv2/imgproc.hpp>

#include "slideio/slideio/slideio.hpp"
#include "slideio/base/resolution.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/imagetools/smallimage.hpp"


slideio::GDALScene::GDALScene(SmallImagePage* page, const std::string& path) : 
    m_imagePage(page), 
    m_filePath(path)
{
    m_filePath = path;
}


std::string slideio::GDALScene::getFilePath() const
{
    return m_filePath;
}


int slideio::GDALScene::getNumChannels() const
{
    if (m_imagePage == nullptr) {
        RAISE_RUNTIME_ERROR << "GDALDriver: Invalid image header by channel number query";
    }
    return m_imagePage->getNumChannels();
}

slideio::DataType slideio::GDALScene::getChannelDataType(int channel) const
{
    if (m_imagePage == nullptr) {
        RAISE_RUNTIME_ERROR << "GDALDriver: Invalid image header by data type query";
    }
    return m_imagePage->getDataType();
}

slideio::Resolution  slideio::GDALScene::getResolution() const
{
    if (m_imagePage == nullptr) {
        RAISE_RUNTIME_ERROR << "GDALDriver: Invalid image header by data type query";
    }
    return m_imagePage->getResolution();
}

double slideio::GDALScene::getMagnification() const
{
    return 0;
}


std::string slideio::GDALScene::getName() const
{
    return std::string();
}

cv::Rect slideio::GDALScene::getRect() const
{
    if (m_imagePage == nullptr) {
        RAISE_RUNTIME_ERROR << "GDALDriver: Invalid image header by scene size query";
    }
    return { {}, m_imagePage->getSize() };
}

void slideio::GDALScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
	if (zSliceIndex != 0 || tFrameIndex != 0) {
		RAISE_RUNTIME_ERROR << "GDALDriver: Z-slice and T-frame indices are not supported";
	}
    if(m_imagePage == nullptr) {
        RAISE_RUNTIME_ERROR << "GDALDriver: Invalid file header by raster reading operation";
    }
    const int numChannels = m_imagePage->getNumChannels();
    auto channelIndices = Tools::completeChannelList(componentIndices, numChannels);
    cv::Mat sceneRaster;
    if (Tools::isConsecutiveFromZero(channelIndices, numChannels)) {
        m_imagePage->readRaster(sceneRaster);
    } else {
        cv::Mat raster;
        m_imagePage->readRaster(raster);
        std::vector<cv::Mat> channels(channelIndices.size());
        int channelNum = 0;
        for (const auto& channelIndex : channelIndices) {
            cv::extractChannel(raster, channels[channelNum++], channelIndex);
        }
        cv::merge(channels, sceneRaster);
    }
    cv::Mat blockRaster = sceneRaster(blockRect);
    if ((blockSize.width != blockRect.width) || (blockSize.height != blockRect.height)) {
        cv::resize(blockRaster, blockRaster, blockSize, 0, 0, cv::INTER_LINEAR);
    }
    blockRaster.copyTo(output);
}

slideio::Compression slideio::GDALScene::getCompression() const {
    if (m_imagePage == nullptr) {
        RAISE_RUNTIME_ERROR << "GDALDriver: Invalid image header by compression query";
    }
    return m_imagePage->getCompression();
}

slideio::MetadataFormat slideio::GDALScene::getMetadataFormat() const {
    return MetadataFormat::JSON;
}

std::string slideio::GDALScene::getRawMetadata() const {
    return m_imagePage->getMetadata();
}
