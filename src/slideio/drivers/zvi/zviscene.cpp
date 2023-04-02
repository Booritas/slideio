// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/zvi/zviscene.hpp"
#include "slideio/drivers/zvi/zvislide.hpp"
#include "slideio/drivers/zvi/zvitags.hpp"
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/variant.hpp>

#include "zviutils.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/imagetools/cvtools.hpp"
#include "slideio/imagetools/imagetools.hpp"

using namespace slideio;


ZVIScene::ZVIScene(const std::string& filePath) :
    m_filePath(filePath),
#if defined(WIN32)
    m_Doc(Tools::toWstring(filePath)),
#else
    m_Doc(filePath),
#endif
    m_SceneName("Unknown")
{
    init();
}

std::string ZVIScene::getFilePath() const
{
    return m_filePath;
}

cv::Rect ZVIScene::getRect() const
{
    return cv::Rect(0, 0, m_Width, m_Height);
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
    if (channel < 0 || channel >= m_ChannelCount)
    {
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
    readResampledBlockChannelsEx(blockRect, blockSize, getValidChannelIndices(componentIndices), 0, 0, output);
}

void ZVIScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
                                            const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex,
                                            cv::OutputArray output)
{
    TilerData userData;
    userData.zSliceIndex = zSliceIndex;
    TileComposer::composeRect(this, componentIndices, blockRect, blockSize, output, &userData);
}


std::string ZVIScene::getName() const
{
    return m_SceneName;
}

Compression ZVIScene::getCompression() const
{
    return m_Compression;
}

int ZVIScene::getTileCount(void* userData)
{
    return m_TileCountX * m_TileCountY;
}

bool ZVIScene::getTileRect(int tileIndex, cv::Rect& tileRect, void* userData)
{
    tileRect = m_Tiles[tileIndex].getRect();
    return true;
}

bool ZVIScene::readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
                        void* userData)
{
    TilerData* data = (TilerData*)userData;
    int slice = data->zSliceIndex;
    ZVITile& tile = m_Tiles[tileIndex];
    return tile.readTile(channelIndices, tileRaster, slice, m_Doc);
}


ZVIPixelFormat ZVIScene::getPixelFormat() const
{
    return (m_PixelFormat == ZVIPixelFormat::PF_UNKNOWN) ? m_ImageItems[0].getPixelFormat() : m_PixelFormat;
}

void ZVIScene::alignChannelInfoToPixelFormat()
{
    if (m_ChannelCount == 1 && !m_ImageItems.empty())
    {
        ZVIPixelFormat pixelFormat = getPixelFormat();
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
    int minChannel = std::numeric_limits<int>::max();

    for (auto&& imageItem : m_ImageItems)
    {
        const int channelIndex = imageItem.getCIndex();
        maxChannel = std::max(channelIndex, maxChannel);
        minChannel = std::min(channelIndex, minChannel);
        maxZ = std::max(imageItem.getZIndex(), maxZ);
        maxT = std::max(imageItem.getTIndex(), maxT);
    }

    m_ChannelCount = maxChannel - minChannel + 1;
    m_ZSliceCount = maxZ + 1;
    m_TFrameCount = maxT + 1;
    m_ChannelNames.resize(m_ChannelCount);
    m_ChannelDataTypes.resize(m_ChannelCount);

    for (auto&& imageItem : m_ImageItems)
    {
        int channelIndex = imageItem.getCIndex();
        if (minChannel > 0) {
            channelIndex -= minChannel;
            imageItem.setCIndex(channelIndex);
        }
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

    for (auto itemIndex = 0; itemIndex < m_RawCount; ++itemIndex)
    {
        auto& item = m_ImageItems[itemIndex];
        item.setItemIndex(itemIndex);
        item.readItemInfo(m_Doc);
        const int validBits = item.getValidBits();
        if (validBits==0 || validBits==1) {
            m_Compression = Compression::Jpeg;
        }
    }
}

void ZVIScene::parseImageInfo()
{
    ZVIUtils::StreamKeeper stream(m_Doc, "/Image/Contents");
    ZVIUtils::skipItems(stream, 4);
    m_Width = ZVIUtils::readIntItem(stream);
    m_Height = ZVIUtils::readIntItem(stream);
    ZVIUtils::skipItem(stream);
    m_PixelFormat = (ZVIPixelFormat)ZVIUtils::readIntItem(stream);
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
    int yPos = 0;
    int tileIndex = 0;
    for (int yIndex = 0; yIndex < m_TileCountY; ++yIndex)
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
    Tools::throwIfPathNotExist(m_filePath, "ZVIScene::init");
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
    switch (units)
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

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"
#endif

void ZVIScene::parseImageTags()
{
    ZVIUtils::StreamKeeper stream(m_Doc, "/Image/Tags/Contents");
    const int version = ZVIUtils::readIntItem(stream);
    const int numTags = ZVIUtils::readIntItem(stream);
    double scaleX(0), scaleY(0), scaleZ(0);
    int unitsX(0), unitsY(0), unitsZ(0);

    for (int tagIndex = 0; tagIndex < numTags; ++tagIndex)
    {
        const ZVIUtils::Variant tag = ZVIUtils::readItem(stream);
        const ZVITAG id = static_cast<ZVITAG>(ZVIUtils::readIntItem(stream));
        ZVIUtils::skipItem(stream);
        if (tag.which() == 0)
            continue;

        switch (id)
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
        case ZVITAG::ZVITAG_FILE_NAME:
            m_SceneName = boost::get<std::string>(tag);
            break;
        case ZVITAG::ZVITAG_COMPRESSION:
            break;
        }
    }
    m_res.x = scaleToResolution(scaleX, unitsX);
    m_res.y = scaleToResolution(scaleY, unitsY);
    m_ZSliceRes = scaleToResolution(scaleZ, unitsZ);
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

