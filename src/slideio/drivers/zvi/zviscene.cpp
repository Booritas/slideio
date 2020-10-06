// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/zvi/zviscene.hpp"
#include "slideio/drivers/zvi/zvislide.hpp"
#include "slideio/drivers/zvi/zvitags.hpp"
#include <boost/filesystem.hpp>
#include <pole/polepp.hpp>
#include <boost/format.hpp>
#include <boost/variant.hpp>

#include "ZVIUtils.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/imagetools/tools.hpp"

using namespace slideio;


ZVIScene::ZVIScene(const std::string& filePath) :
                        m_filePath(filePath),
                        m_Doc(filePath)
{
    init();
}

std::string ZVIScene::getFilePath() const
{
    return m_filePath;
}

cv::Rect ZVIScene::getRect() const
{
    return cv::Rect(0,0, m_Width, m_Height);
}

int ZVIScene::getNumChannels() const
{
    return m_ChannelCount;
}

int ZVIScene::getNumZSlices() const
{
    return m_ZSliceCount;
}

int ZVIScene::getNumTFrames() const
{
    return m_TFrameCount;
}

double ZVIScene::getZSliceResolution() const
{
    return m_ZSliceRes;
}

double ZVIScene::getTFrameResolution() const
{
    return 0;
}

void ZVIScene::validateChannelIndex(int channel) const
{
    if(channel<0 || channel>=m_ChannelCount) {
        throw std::runtime_error(
            (boost::format("Invalid channel index: %1%. Number of channels: %2%")
                % channel % m_ChannelCount).str()
        );
    }
}

slideio::DataType ZVIScene::getChannelDataType(int channel) const
{
    validateChannelIndex(channel);
    return m_ChannelDataTypes[channel];
}

std::string ZVIScene::getChannelName(int channel) const
{
    validateChannelIndex(channel);
    return m_ChannelNames[channel];
}

Resolution ZVIScene::getResolution() const
{
    return m_res;
}

double ZVIScene::getMagnification() const
{
    return 0;
}

void ZVIScene::readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& componentIndices, cv::OutputArray output)
{
    readResampledBlockChannelsEx(blockRect, blockSize, componentIndices, 0, 0, output);
}

void ZVIScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
    //TilerData userData;
    //const double zoomX = static_cast<double>(blockSize.width) / static_cast<double>(blockRect.width);
    //const double zoomY = static_cast<double>(blockSize.height) / static_cast<double>(blockRect.height);
    //const double zoom = std::max(zoomX, zoomY);
    //const std::vector<ZoomLevel>& zoomLevels = m_zoomLevels;
    //userData.zoomLevelIndex = Tools::findZoomLevel(zoom, static_cast<int>(m_zoomLevels.size()), [&zoomLevels](int index) {
    //    return zoomLevels[index].zoom;
    //    });
    //const double levelZoom = zoomLevels[userData.zoomLevelIndex].zoom;
    //cv::Rect zoomLevelRect;
    //ImageTools::scaleRect(blockRect, levelZoom, levelZoom, zoomLevelRect);
    //userData.relativeZoom = levelZoom / zoom;
    //userData.zSliceIndex = zSliceIndex;
    //userData.tFrameIndex = tFrameIndex;
    //TileComposer::composeRect(this, componentIndices, zoomLevelRect, blockSize, output, &userData);
}

void ZVIScene::readResampled4DBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& channelIndices, const cv::Range& zSliceRange, const cv::Range& timeFrameRange,
    cv::OutputArray output)
{
    std::vector<cv::Mat> rasters;
    for (int tfIndex = timeFrameRange.start; tfIndex < timeFrameRange.end; ++tfIndex)
    {
        for (int zSlieceIndex = zSliceRange.start; zSlieceIndex < zSliceRange.end; ++zSlieceIndex)
        {
            cv::Mat raster;
            readResampledBlockChannelsEx(blockRect, blockSize, channelIndices, zSlieceIndex, tfIndex, raster);
            rasters.push_back(raster);
        }
    }
    cv::merge(rasters, output);
}

std::string ZVIScene::getName() const
{
    return "";
}

Compression ZVIScene::getCompression() const
{
    return Compression::Unknown;
}

int ZVIScene::getTileCount(void* userData)
{
    return 0;
}

bool ZVIScene::getTileRect(int tileIndex, cv::Rect& tileRect, void* userData)
{
    return false;
}

bool ZVIScene::readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
    void* userData)
{
    return false;
}


void ZVIScene::alignChannelInfoToPixelFormat()
{
    if(m_ChannelCount==1 && !m_ImageItems.empty())
    {
        ZVIPixelFormat pixelFormat = m_ImageItems[0].getPixelFormat();
        switch (pixelFormat)
        {
        case ZVIPixelFormat::PF_BGR:
            m_ChannelCount = 3;
            m_ChannelNames.resize(m_ChannelCount);
            m_ChannelDataTypes.resize(m_ChannelCount);
            std::fill(m_ChannelDataTypes.begin(), m_ChannelDataTypes.end(), DataType::DT_Byte);
            m_ChannelNames[0] = "blue";
            m_ChannelNames[1] = "green";
            m_ChannelNames[2] = "red";
            break;
        case ZVIPixelFormat::PF_BGR16:
            m_ChannelCount = 3;
            m_ChannelDataTypes.resize(m_ChannelCount);
            std::fill(m_ChannelDataTypes.begin(), m_ChannelDataTypes.end(), DataType::DT_Int16);
            m_ChannelNames.resize(m_ChannelCount);
            m_ChannelNames[0] = "blue";
            m_ChannelNames[1] = "green";
            m_ChannelNames[2] = "red";
            break;
        case ZVIPixelFormat::PF_BGR32:
            m_ChannelCount = 3;
            m_ChannelDataTypes.resize(m_ChannelCount);
            std::fill(m_ChannelDataTypes.begin(), m_ChannelDataTypes.end(), DataType::DT_Int32);
            m_ChannelNames.resize(m_ChannelCount);
            m_ChannelNames[0] = "blue";
            m_ChannelNames[1] = "green";
            m_ChannelNames[2] = "red";
            break;
        case ZVIPixelFormat::PF_BGRA:
            m_ChannelCount = 4;
            m_ChannelDataTypes.resize(m_ChannelCount);
            std::fill(m_ChannelDataTypes.begin(), m_ChannelDataTypes.end(), DataType::DT_Byte);
            m_ChannelNames.resize(m_ChannelCount);
            m_ChannelNames[0] = "blue";
            m_ChannelNames[1] = "green";
            m_ChannelNames[2] = "red";
            m_ChannelNames[2] = "alpha";
            break;
        case ZVIPixelFormat::PF_UINT8:
        case ZVIPixelFormat::PF_INT16:
        case ZVIPixelFormat::PF_INT32:
        case ZVIPixelFormat::PF_FLOAT:
        case ZVIPixelFormat::PF_DOUBLE:
            break;
        case ZVIPixelFormat::PF_UNKNOWN:
        default:
            throw std::runtime_error(
                (boost::format("ZVIImageDriver: Invalid pixel format: %1% for file %2%")
                    % (int)pixelFormat % m_filePath).str()
            );
        }
    }
}

void ZVIScene::computeSceneDimensions()
{
    int maxChannel = 0;
    int maxZ = 0;
    int maxT = 0;

    for(auto&& imageItem:m_ImageItems)
    {
        maxChannel = std::max(imageItem.getCIndex(), maxChannel);
        maxZ = std::max(imageItem.getZIndex(), maxZ);
        maxT = std::max(imageItem.getTIndex(), maxT);
    }

    m_ChannelCount = maxChannel + 1;
    m_ZSliceCount = maxZ + 1;
    m_TFrameCount = maxT + 1;
    m_ChannelNames.resize(m_ChannelCount);
    m_ChannelDataTypes.resize(m_ChannelCount);

    for (auto&& imageItem : m_ImageItems)
    {
        const int channelIndex = imageItem.getCIndex();
        const std::string channelName = imageItem.getChannelName();
        if (!channelName.empty())
            m_ChannelNames[channelIndex] = channelName;
        m_ChannelDataTypes[channelIndex] = imageItem.getDataType();
    }

    alignChannelInfoToPixelFormat();
}

void ZVIScene::readImageItems()
{
    m_ImageItems.resize(m_RawCount);
    for(auto itemIndex=0; itemIndex<m_RawCount; ++itemIndex)
    {
        auto& item = m_ImageItems[itemIndex];
        item.setItemIndex(itemIndex);
        item.readItemInfo(m_Doc);
    }
}

void ZVIScene::parseImageInfo()
{
    ZVIUtils::StreamKeeper stream(m_Doc, "/Image/Contents");
    ZVIUtils::skipItems(stream, 8);
    m_RawCount = ZVIUtils::readIntItem(stream);

}

void ZVIScene::computeTiles()
{
    const int tileCount = m_TileCountX * m_TileCountY;
    m_Tiles.resize(tileCount);

    std::vector<int> w(m_TileCountX, -1);
    std::vector<int> h(m_TileCountY, -1);

    for (auto itemIndex = 0; itemIndex < m_ImageItems.size(); ++itemIndex)
    {
        const ZVIImageItem& item = m_ImageItems[itemIndex];
        int xIndex = item.getTileIndexX();
        int yIndex = item.getTileIndexY();
        int tileIndex = yIndex * m_TileCountX + xIndex;
        ZVITile& tile = m_Tiles[tileIndex];
        tile.addItem(&item);
        if (w[xIndex] < 0)
            w[xIndex] = item.getWidth();
        if (h[yIndex] < 0)
            h[yIndex] = item.getHeight();
    }
    int yPos = 0; int tileIndex = 0;
    for (int yIndex=0; yIndex<m_TileCountY; ++yIndex)
    {
        int xPos = 0;
        for (int xIndex = 0; xIndex < m_TileCountX; ++xIndex)
        {
            ZVITile& tile = m_Tiles[tileIndex];
            tile.setTilePosition(xPos, yPos);
            tile.finalize();
            xPos += w[xIndex];
            tileIndex++;
        }
        yPos += h[yIndex];
    }   
}

void ZVIScene::init()
{
    namespace fs = boost::filesystem;
    if (!fs::exists(m_filePath)) {
        throw std::runtime_error(std::string("ZVIImageDriver: File does not exist:") + m_filePath);
    }
    if (!m_Doc.good())
    {
        throw std::runtime_error(
            (boost::format("Cannot open compound file %1%") % m_filePath).str());
    }
    parseImageInfo();
    readImageItems();
    computeSceneDimensions();
    parseImageTags();
    computeTiles();
}

static double scaleToResolution(double scale, int units)
{
    double res = scale;
    switch(units)
    {
    case 72: // Meter
        break;
    case 76: // Micrometer
    case 84: // Micrometer
        res /= 1.e6;
        break;
    case 77: // Namometer
        res /= 1.e9;
        break;
    }
    return res;
}

void ZVIScene::parseImageTags()
{
    ZVIUtils::StreamKeeper stream(m_Doc, "/Image/Tags/Contents");
    const int version = ZVIUtils::readIntItem(stream);
    const int numTags = ZVIUtils::readIntItem(stream);
    double scaleX(0), scaleY(0), scaleZ(0);
    int unitsX(0), unitsY(0), unitsZ(0);

    for(int tagIndex=0; tagIndex<numTags; ++tagIndex)
    {
        const ZVIUtils::Variant tag = ZVIUtils::readItem(stream);
        const ZVITAG id = static_cast<ZVITAG>(ZVIUtils::readIntItem(stream));
        ZVIUtils::skipItem(stream);

        switch(id)
        {
        case ZVITAG::ZVITAG_IMAGE_WIDTH:
            m_Width = boost::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_IMAGE_HEIGHT:
            m_Height = boost::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_IMAGE_COUNT_U:
            m_TileCountX = boost::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_IMAGE_COUNT_V:
            m_TileCountY = boost::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_SCALE_X:
            scaleX = boost::get<double>(tag);
            break;
        case ZVITAG::ZVITAG_SCALE_UNIT_X:
            unitsX = boost::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_SCALE_Y:
            scaleY = boost::get<double>(tag);
            break;
        case ZVITAG::ZVITAG_SCALE_UNIT_Y:
            unitsY = boost::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_SCALE_Z:
            scaleZ = boost::get<double>(tag);
            break;
        case ZVITAG::ZVITAG_SCALE_UNIT_Z:
            unitsZ = boost::get<int32_t>(tag);
            break;
        }
    }
    m_res.x = scaleToResolution(scaleX, unitsX);
    m_res.y = scaleToResolution(scaleY, unitsY);
    m_ZSliceRes = scaleToResolution(scaleZ, unitsZ);
}
