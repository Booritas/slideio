// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/exceptions.hpp"
#include "slideio/drivers/gdal/gdalscene.hpp"

#include <opencv2/imgproc.hpp>

#include "slideio/slideio/slideio.hpp"
#include "slideio/core/resolution.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/core/levelinfo.hpp"
#include "slideio/imagetools/smallimage.hpp"
#include "slideio/core/tools/cvtools.hpp"

#include <algorithm>
#include <numeric>


slideio::GDALScene::GDALScene(const std::shared_ptr<SmallImage>& image, int pageIndex, const std::string& path,
                              const std::string& driverId) :
    m_image(image),
    m_imagePage(image->readPage(pageIndex)),
    m_filePath(path),
	m_driverId(driverId)
{
    m_filePath = path;
    // A gdal image has no pyramid, but a scene with no level cannot be addressed by level
    // at all, so it registers the single level it is. Same shape as SVSSmallScene.
    if (m_imagePage != nullptr) {
        if (const auto acquired = Tools::parseTiffDateTime(m_imagePage->getAcquisitionDateTime())) {
            m_acquisitionTime = *acquired;
        }
        const cv::Size imageSize = m_imagePage->getSize();
        // The page has to be decoded whole whatever is asked of it, and says so by
        // reporting itself as its one tile.
        const cv::Size tileSize = imageSize;
        LevelInfo level;
        level.setLevel(0);
        level.setScale(1.);
        level.setMagnification(getMagnification());
        level.setSize({imageSize.width, imageSize.height});
        level.setTileSize({tileSize.width, tileSize.height});
        m_levels.push_back(level);
        // Bytes only here: the driver never parses the profile it finds, it
        // just surfaces whatever FreeImage already extracted for this page.
        m_colorProfile = ColorProfile(m_imagePage->getICCProfile());
    }
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
    const auto channelIndices = Tools::completeChannelList(componentIndices, numChannels);
    readClampedBlock(cv::Rect(cv::Point(), m_imagePage->getSize()), blockRect, blockSize, componentIndices,
        [&](const cv::Rect& rect, const cv::Size& size, cv::OutputArray block) {
            readWholePageBlock(rect, size, channelIndices, block);
        },
        output);
}

void slideio::GDALScene::readWholePageBlock(const cv::Rect& rect, const cv::Size& size,
                                            const std::vector<int>& channelIndices, cv::OutputArray block)
{
    const int numChannels = m_imagePage->getNumChannels();
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
    cv::Mat blockRaster = sceneRaster(rect);
    if ((size.width != rect.width) || (size.height != rect.height)) {
        cv::resize(blockRaster, blockRaster, size, 0, 0, cv::INTER_LINEAR);
    }
    blockRaster.copyTo(block);
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
