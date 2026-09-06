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
#include "slideio/base/exceptions.hpp"
#include "slideio/base/log.hpp"

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
    // {PositionInformation}: a length prefixed blob of at least seven 32-bit
    // fields. Indexing it without checking the length read a blob shorter than
    // that out of bounds -- and wrote the byte swapped values back.
    const int positionFields = 7;
    uint16_t type = 0;
    ZVIUtils::readExactly(stream, &type, sizeof(type));
	type=Endian::fromLittleEndianToNative(type);

    uint32_t sz = 0;
    ZVIUtils::readExactly(stream, &sz, sizeof(sz));
	sz = Endian::fromLittleEndianToNative(sz);
    if (sz < positionFields * sizeof(uint32_t))
    {
        RAISE_RUNTIME_ERROR << "ZVIImageDriver: " << streamPath
            << ": position information is " << sz << " bytes, expected at least "
            << positionFields * sizeof(uint32_t);
    }
    std::vector<char> posBuffer(sz);
    ZVIUtils::readExactly(stream, posBuffer.data(), posBuffer.size());
    uint32_t* position = reinterpret_cast<uint32_t*>(posBuffer.data());
	for(int index=0; index<positionFields; ++index)
		position[index] = Endian::fromLittleEndianToNative(position[index]);

    setZIndex(position[2]);
    setCIndex(position[3]);
    setTIndex(position[4]);
    setSceneIndex(position[5]);
    setPositionIndex(position[6]);

    ZVIUtils::skipItems(stream, 5);
    std::vector<int32_t> header(7);
    ZVIUtils::readExactly(stream, header.data(), sizeof(int32_t) * header.size());
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
        // {NumberOfTags} is not always the number of tags the stream holds, and
        // a tag the reader cannot decode must not cost the item the tags before
        // it: without its tags the item has no tile index and the file will not
        // open at all.
        if (ZVIUtils::bytesLeft(stream) < 2) {
            SLIDEIO_LOG(WARNING) << "ZVIImageDriver: " << streamPath << " ends after "
                << tagIndex << " of " << numTags << " declared tags";
            break;
        }
        ZVIUtils::Variant tag;
        ZVITAG id = static_cast<ZVITAG>(0); // no tag has id 0
        try {
            tag = ZVIUtils::readItem(stream);
            id = static_cast<ZVITAG>(ZVIUtils::readIntItem(stream));
            ZVIUtils::skipItem(stream);
        }
        catch (const std::exception& e) {
            SLIDEIO_LOG(WARNING) << "ZVIImageDriver: stopped reading " << streamPath
                << " after " << tagIndex << " of " << numTags << " tags: " << e.what();
            break;
        }
        if (tag.index() == 0) {
            continue; // std::monostate: nothing the cases below can read.
        }

        try {
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
        case ZVITAG::ZVITAG_MULTICHANNEL_COLOUR:
            // Windows-style packed BGR (0x00BBGGRR). Drivers may report it
            // as signed or unsigned; both shapes map to the same 32 bits.
            if (auto* p = std::get_if<int32_t>(&tag)) {
                m_MultichannelColour = *p;
            } else if (auto* p = std::get_if<uint32_t>(&tag)) {
                m_MultichannelColour = static_cast<int>(*p);
            }
            break;
        case ZVITAG::ZVITAG_EMISSION_WAVELENGTH:
            if (auto* p = std::get_if<int32_t>(&tag)) {
                m_EmissionWavelength = static_cast<double>(*p);
            } else if (auto* p = std::get_if<double>(&tag)) {
                m_EmissionWavelength = *p;
            }
            break;
        case ZVITAG::ZVITAG_EXCITATION_WAVELENGTH:
            if (auto* p = std::get_if<int32_t>(&tag)) {
                m_ExcitationWavelength = static_cast<double>(*p);
            } else if (auto* p = std::get_if<double>(&tag)) {
                m_ExcitationWavelength = *p;
            }
            break;
        case ZVITAG::ZVITAG_REFLECTOR:
            if (auto* p = std::get_if<std::string>(&tag)) {
                m_Reflector = *p;
            }
            break;
        }
        }
        catch (const std::bad_variant_access&) {
            // The tag carries a type this case does not expect. The stream is
            // still in sync, so only this one tag is lost.
            SLIDEIO_LOG(WARNING) << "ZVIImageDriver: " << streamPath << ": tag "
                << static_cast<int>(id) << " has an unexpected value type";
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
