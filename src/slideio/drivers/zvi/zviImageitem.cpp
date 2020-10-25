// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <boost/format.hpp>

#include "slideio/core/cvglobals.hpp"
#include "slideio/drivers/zvi/zvitags.hpp"
#include "slideio/drivers/zvi/zviutils.hpp"
#include "slideio/drivers/zvi/zviimageitem.hpp"

using namespace slideio;

void ZVIImageItem::readItemInfo(ole::compound_document& doc)
{
    readContents(doc);
    readTags(doc);
}


void ZVIImageItem::readContents(ole::compound_document& doc)
{
    const std::string streamPath = (boost::format("/Image/Item(%1%)/Contents") % getItemIndex()).str();
    ZVIUtils::StreamKeeper stream(doc, streamPath);

    ZVIUtils::skipItems(stream, 11);
    uint16_t type;
    stream->read(reinterpret_cast<char*>(&type), sizeof(type));
    uint32_t sz;
    stream->read(reinterpret_cast<char*>(&sz), sizeof(sz));
    std::vector<char> posBuffer(sz);
    stream->read(posBuffer.data(), posBuffer.size());
    const uint32_t* position = reinterpret_cast<uint32_t*>(posBuffer.data());

    setZIndex(position[2]);
    setCIndex(position[3]);
    setTIndex(position[4]);
    setSceneIndex(position[5]);
    setPositionIndex(position[6]);

    ZVIUtils::skipItems(stream, 5);
    std::vector<int32_t> header(6);
    stream->read(reinterpret_cast<char*>(header.data()), sizeof(int32_t) * header.size());
    const int32_t version = header[0];
    const int32_t width = header[1];
    const int32_t height = header[2];
    const int32_t depth = header[3];
    const ZVIPixelFormat pixelFormat = static_cast<ZVIPixelFormat>(header[5]);

    setPixelFormat(pixelFormat);
    DataType dt = ZVIUtils::dataTypeFromPixelFormat(pixelFormat);
    int channels = ZVIUtils::channelCountFromPixelFormat(pixelFormat);
    setDataType(dt);
    setHeight(height);
    setWidth(width);
    setZSliceCount(depth);
    std::streamoff pos = stream->pos();
    setDataOffset(pos);
}


void ZVIImageItem::readTags(ole::compound_document& doc)
{
    const std::string streamPath = (boost::format("/Image/Item(%1%)/Tags/Contents") % getItemIndex()).str();
    ZVIUtils::StreamKeeper stream(doc, streamPath);

    const int version = ZVIUtils::readIntItem(stream);
    const int numTags = ZVIUtils::readIntItem(stream);
    int32_t itemWidth = 0;
    int32_t itemHeight = 0;
    int32_t itemTilesX = 0;
    int32_t itemTilesY = 0;
    int32_t itemTileIndexX = 0;
    int32_t itemTileIndexY = 0;
    int32_t imageTileIndex = 0;
    std::string channelName;
    for (int tagIndex = 0; tagIndex < numTags; ++tagIndex)
    {
        ZVIUtils::Variant tag = ZVIUtils::readItem(stream);
        ZVITAG id = static_cast<ZVITAG>(ZVIUtils::readIntItem(stream));
        ZVIUtils::skipItem(stream);

        switch (id)
        {
        case ZVITAG::ZVITAG_IMAGE_TILE_INDEX:
            imageTileIndex = boost::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_IMAGE_WIDTH:
            itemWidth = boost::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_IMAGE_HEIGHT:
            itemHeight = boost::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_IMAGE_COUNT:
            break;
        case ZVITAG::ZVITAG_IMAGE_PIXEL_TYPE:
            break;
        case ZVITAG::ZVITAG_IMAGE_INDEX_U:
            itemTileIndexX = boost::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_IMAGE_INDEX_V:
            itemTileIndexY = boost::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_IMAGE_COUNT_U:
            itemTileIndexY = boost::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_IMAGE_COUNT_V:
            itemTilesY = boost::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_CHANNEL_NAME:
            channelName = boost::get<std::string>(tag);
            break;
        }
    }

    setChannelName(channelName);
    setTileIndexX(itemTileIndexX);
    setTileIndexY(itemTileIndexY);
}

cv::Mat ZVIImageItem::readRaster(ole::compound_document& doc) const
{
    cv::Mat raster;
    const std::string streamPath = (boost::format("/Image/Item(%1%)/Contents") % getItemIndex()).str();
    ZVIUtils::StreamKeeper stream(doc, streamPath);
    stream->seek( getDataOffset(), std::ios::beg);
    raster.create(m_Height, m_Width, CV_MAKETYPE(toOpencvType(m_DataType), m_ChannelCount));
    size_t bytes = raster.total() * raster.elemSize();
    stream->read(reinterpret_cast<char*>(raster.data), bytes);
    return raster;
}
