// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <fstream>
#include "slideio/core/tools/cvtools.hpp"
#include "slideio/drivers/zvi/zvitags.hpp"
#include "slideio/drivers/zvi/zviutils.hpp"
#include "slideio/drivers/zvi/zviimageitem.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/core/tools/endian.hpp"

using namespace slideio;

void ZVIImageItem::readItemInfo(ole::compound_document& doc)
{
    readContents(doc);
    readTags(doc);
}


void ZVIImageItem::readContents(ole::compound_document& doc)
{
    const std::string streamPath = std::string("/Image/Item(") + std::to_string(getItemIndex()) + ")/Contents";
    ZVIUtils::StreamKeeper stream(doc, streamPath);

    ZVIUtils::skipItems(stream, 11);
    uint16_t type;
    stream->read(reinterpret_cast<char*>(&type), sizeof(type));
	type=Endian::fromLittleEndianToNative(type);

    uint32_t sz;
    stream->read(reinterpret_cast<char*>(&sz), sizeof(sz));
	sz = Endian::fromLittleEndianToNative(sz);
    std::vector<char> posBuffer(sz);
    stream->read(posBuffer.data(), posBuffer.size());
    uint32_t* position = reinterpret_cast<uint32_t*>(posBuffer.data());
	for(int index=0; index<7; ++index)
		position[index] = Endian::fromLittleEndianToNative(position[index]);

    setZIndex(position[2]);
    setCIndex(position[3]);
    setTIndex(position[4]);
    setSceneIndex(position[5]);
    setPositionIndex(position[6]);

    ZVIUtils::skipItems(stream, 5);
    std::vector<int32_t> header(7);
    stream->read(reinterpret_cast<char*>(header.data()), sizeof(int32_t) * header.size());
	for(int index=0; index<header.size(); ++index)
		header[index] = Endian::fromLittleEndianToNative(header[index]);
    const int32_t version = header[0];
    const int32_t width = header[1];
    const int32_t height = header[2];
    const int32_t depth = header[3];
    const auto pixelFormat = static_cast<ZVIPixelFormat>(header[5]);
    const int32_t validBits = header[6];

    setPixelFormat(pixelFormat);
    setHeight(height);
    setWidth(width);
    setZSliceCount(depth);
    setValidBits(validBits);
    std::streamoff pos = stream->pos();
    setDataOffset(pos);
}

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"
#endif

void ZVIImageItem::readTags(ole::compound_document& doc)
{
    const std::string streamPath = std::string("/Image/Item(") + std::to_string(getItemIndex()) + ")/Tags/Contents";
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
            imageTileIndex = std::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_IMAGE_WIDTH:
            itemWidth = std::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_IMAGE_HEIGHT:
            itemHeight = std::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_IMAGE_COUNT:
            break;
        case ZVITAG::ZVITAG_IMAGE_PIXEL_TYPE:
            break;
        case ZVITAG::ZVITAG_IMAGE_INDEX_U:
            itemTileIndexX = std::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_IMAGE_INDEX_V:
            itemTileIndexY = std::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_IMAGE_COUNT_U:
            itemTileIndexY = std::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_IMAGE_COUNT_V:
            itemTilesY = std::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_CHANNEL_NAME:
            channelName = std::get<std::string>(tag);
            break;
        }
    }

    setChannelName(channelName);
    setTileIndexX(itemTileIndexX);
    setTileIndexY(itemTileIndexY);
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

void ZVIImageItem::readRaster(ole::compound_document& doc, cv::OutputArray raster) const
{
    const DataType dt = getDataType();
    const int ds = CVTools::cvGetDataTypeSize(dt);
    const size_t pixels = getWidth() * getHeight();
    const int channels = getChannelCount();
    const std::streamoff rasterSize = pixels * ds * channels;
    const int validBites = getValidBits();
    const ZVIPixelFormat pixelFormat = getPixelFormat();


    const std::string streamPath = std::string("/Image/Item(") + std::to_string(getItemIndex()) + ")/Contents";
    ZVIUtils::StreamKeeper stream(doc, streamPath);

    stream->seek(getDataOffset(), std::ios::beg);

    if (validBites==0 || validBites==1)
    {
        stream->seek(0, std::ios::end);
        std::streampos endPos = stream->pos();
        std::streamsize bytesToRead = endPos - getDataOffset();
        stream->seek(getDataOffset(), std::ios::beg);
        std::vector<uint8_t> buff(bytesToRead);
        stream->read(reinterpret_cast<char*>(buff.data()), bytesToRead);
        ImageTools::decodeJpegStream(buff.data(), buff.size(), raster);
    }
    else
    {
        raster.create(getHeight(), getWidth(), CV_MAKETYPE(CVTools::toOpencvType(dt), channels));
        cv::Mat& mat = raster.getMatRef();

        stream->seek(getDataOffset(), std::ios::beg);
        const auto readBytes = stream->read(reinterpret_cast<char*>(mat.data), rasterSize);
        if (readBytes != rasterSize) {
            throw std::runtime_error("ZVIImageDriver: Unexpected end of stream");
        }
        Endian::fromLittleEndianToNative(dt, mat.data, readBytes);
    }

}

void ZVIImageItem::setPixelFormat(ZVIPixelFormat pixelFormat)
{
    m_PixelFormat = pixelFormat;
    m_ChannelCount = ZVIUtils::channelCountFromPixelFormat(pixelFormat);
    m_DataType = ZVIUtils::dataTypeFromPixelFormat(pixelFormat);
}
