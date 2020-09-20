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
                        m_ChannelCount(0),
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
    return 0;
}

int ZVIScene::getNumTFrames() const
{
    return 0;
}

double ZVIScene::getZSliceResolution() const
{
    return 0;
}

double ZVIScene::getTFrameResolution() const
{
    return 0;
}

slideio::DataType ZVIScene::getChannelDataType(int channel) const
{
    return m_ChannelDataType;
}

std::string ZVIScene::getChannelName(int channel) const
{
    return m_ChannelNames[channel];
}

Resolution ZVIScene::getResolution() const
{
    return Resolution();
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

void ZVIScene::readImageItem(ImageItem& item)
{
    const std::string streamPath = (boost::format("/Image/Item(%1%)/Contents") % item.getItemIndex()).str();
    ZVIUtils::StreamKeeper stream(m_Doc, streamPath);
    //ZVIUtils::skipItems(stream, 4);
    //const int itemWidth = ZVIUtils::readIntItem(stream);
    //const int itemHeight = ZVIUtils::readIntItem(stream);
    //const int itemDepth = ZVIUtils::readIntItem(stream);
    //const int itemPixelFormat = ZVIUtils::readIntItem(stream);
    //ZVIUtils::skipItem(stream);
    //const int validBitsPerPixel = ZVIUtils::readIntItem(stream);
    //ZVIUtils::skipItem(stream);

    ZVIUtils::skipItems(stream, 11);
    uint16_t type;
    stream->read(reinterpret_cast<char*>(&type), sizeof(type));
    uint32_t sz;
    stream->read(reinterpret_cast<char*>(&sz), sizeof(sz));
    std::array<int32_t, 8> position{};
    stream->read(reinterpret_cast<char*>(position.data()), sizeof(int32_t)*position.size());
    item.setZIndex(position[3]);
    item.setCIndex(position[4]);
    item.setTIndex(position[5]);
    item.setSceneIndex(position[6]);
    item.setPositionIndex(position[7]);
}

void ZVIScene::readImageItems()
{
    m_ImageItems.resize(m_RawCount);
    for(auto itemIndex=0; itemIndex<m_RawCount; ++itemIndex)
    {
        auto& item = m_ImageItems[itemIndex];
        item.setItemIndex(itemIndex);
        readImageItem(item);
    }
}

void ZVIScene::parseImageInfo()
{
    ZVIUtils::StreamKeeper stream(m_Doc, "/Image/Contents");
    ZVIUtils::skipItems(stream, 4);
    m_Width = ZVIUtils::readIntItem(stream);
    m_Height = ZVIUtils::readIntItem(stream);
    int depth = ZVIUtils::readIntItem(stream);
    m_PixelFormat = static_cast<PixelFormat>(ZVIUtils::readIntItem(stream));
    m_RawCount = ZVIUtils::readIntItem(stream);

    switch(m_PixelFormat)
    {
    case PixelFormat::PF_BGR:
        m_ChannelCount = 3;
        m_ChannelDataType = DataType::DT_Byte;
        m_ChannelNames.resize(m_ChannelCount);
        m_ChannelNames[0] = "blue";
        m_ChannelNames[1] = "green";
        m_ChannelNames[2] = "red";
        break;
    case PixelFormat::PF_BGR16:
        m_ChannelCount = 3;
        m_ChannelDataType = DataType::DT_Int16;
        m_ChannelNames.resize(m_ChannelCount);
        m_ChannelNames[0] = "blue";
        m_ChannelNames[1] = "green";
        m_ChannelNames[2] = "red";
        break;
    case PixelFormat::PF_BGR32:
        m_ChannelCount = 3;
        m_ChannelDataType = DataType::DT_Int32;
        m_ChannelNames.resize(m_ChannelCount);
        m_ChannelNames[0] = "blue";
        m_ChannelNames[1] = "green";
        m_ChannelNames[2] = "red";
        break;
    case PixelFormat::PF_BGRA:
        m_ChannelCount = 4;
        m_ChannelNames.resize(m_ChannelCount);
        m_ChannelDataType = DataType::DT_Byte;
        m_ChannelNames[0] = "blue";
        m_ChannelNames[1] = "green";
        m_ChannelNames[2] = "red";
        m_ChannelNames[2] = "alpha";
        break;
    case PixelFormat::PF_UINT8:
        m_ChannelCount = 1;
        m_ChannelDataType = DataType::DT_Byte;
        m_ChannelNames.resize(m_ChannelCount);
        m_ChannelNames[0] = "intensity";
        break;
    case PixelFormat::PF_INT16:
        m_ChannelCount = 1;
        m_ChannelDataType = DataType::DT_Int16;
        m_ChannelNames.resize(m_ChannelCount);
        m_ChannelNames[0] = "intensity";
        break;
    case PixelFormat::PF_INT32:
        m_ChannelCount = 1;
        m_ChannelDataType = DataType::DT_Int32;
        m_ChannelNames.resize(m_ChannelCount);
        m_ChannelNames[0] = "intensity";
        break;
    case PixelFormat::PF_FLOAT:
        m_ChannelCount = 1;
        m_ChannelDataType = DataType::DT_Float32;
        m_ChannelNames.resize(m_ChannelCount);
        m_ChannelNames[0] = "intensity";
        break;
    case PixelFormat::PF_DOUBLE:
        m_ChannelCount = 1;
        m_ChannelDataType = DataType::DT_Float64;
        m_ChannelNames.resize(m_ChannelCount);
        m_ChannelNames[0] = "intensity";
        break;
        break;
    case PixelFormat::PF_UNKNOWN:
    default:
        throw std::runtime_error(
            (boost::format("ZVIImageDriver: Invalid pixel format: %1% for file %2%")
                % (int)m_PixelFormat % m_filePath).str()
        );
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
}

void ZVIScene::parseImageTags()
{
    ZVIUtils::StreamKeeper stream(m_Doc, "/Image/Tags/Contents");
    const int version = ZVIUtils::readIntItem(stream);
    const int numTags = ZVIUtils::readIntItem(stream);
    for(int tagIndex=0; tagIndex<numTags; ++tagIndex)
    {
        ZVIUtils::Variant tag = ZVIUtils::readItem(stream);
        ZVITAG id = static_cast<ZVITAG>(ZVIUtils::readIntItem(stream));
        ZVIUtils::skipItem(stream);

        switch(id)
        {
        case ZVITAG_IMAGE_WIDTH:
            break;
        case ZVITAG_IMAGE_HEIGHT:
            break;
        case ZVITAG_IMAGE_COUNT:
            break;
        case ZVITAG_IMAGE_PIXEL_TYPE:
            break;
        case ZVITAG_IMAGE_COUNT_U:
            break;
        case ZVITAG_IMAGE_COUNT_V:
            break;
        }
        
        // std::cout << "Id:" << id << ";  Type:" << tag.which() << "; Value: " << tag << std::endl;
    }
}
