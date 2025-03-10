// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/base/exceptions.hpp"
#include "zviutils.hpp"
#include "zvipixelformat.hpp"
#include <locale>

#include "slideio/core/tools/endian.hpp"
#include "slideio/core/tools/tools.hpp"
using namespace slideio;

void ZVIUtils::skipItem(ole::basic_stream& stream)
{
    uint16_t type;
    stream.read((char*)&type, sizeof(type));
	type = Endian::fromLittleEndianToNative(type);
    uint32_t offset = 0;
    switch(type)
    {
    case VT_EMPTY:
    case VT_NULL:
        break;
    case VT_I1:
    case VT_UI1:
        offset = 1;
        break;
    case VT_I2:
    case VT_UI2:
    case VT_BOOL:
        offset = 2;
        break;
    case VT_I4:
    case VT_INT:
    case VT_UI4:
    case VT_UINT:
        offset = 4;
        break;
    case VT_I8:
    case VT_UI8:
    case VT_DATE:
    case VT_R4:
        offset = 4;
        break;
    case VT_R8:
        offset = 8;
        break;
    case VT_BSTR:
    case VT_ARRAY:
    case VT_BLOB:
    case VT_STORED_OBJECT:
        stream.read((char*)&offset, 4);
		offset = Endian::fromLittleEndianToNative(offset);
        break;
    case VT_DISPATCH:
    case VT_UNKNOWN:
        offset = 16;
        break;
    }
    stream.seek(offset, std::ios::cur);
}

void ZVIUtils::skipItems(ole::basic_stream& stream, int count)
{
   for(int item = 0; item < count; item++)
   {
      skipItem(stream);
   }
}

int32_t ZVIUtils::readIntItem(ole::basic_stream& stream)
{
   uint16_t type(0);
   stream.read((char*)&type, sizeof(type));
   type = Endian::fromLittleEndianToNative(type);
   if(type != VT_I4 && type != VT_INT)
   {
      std::string error =
         "Unexpected data type reading of compound stream. Expected integer. Received:";
      error += std::to_string(type);
      throw std::runtime_error(error);
   }
   int32_t value;
   stream.read((char*)&value, sizeof(value));
   return Endian::fromLittleEndianToNative(value);
}

double ZVIUtils::readDoubleItem(ole::basic_stream& stream)
{
   uint16_t type(0);
   stream.read((char*)&type, sizeof(type));
   type = Endian::fromLittleEndianToNative(type);
   if(type != VT_R8)
   {
      std::string error =
         "Unexpected data type reading of compound stream. Expected VT_R8. Received:";
      error += std::to_string(type);
      throw std::runtime_error(error);
   }
   double value;
   stream.read((char*)&value, sizeof(value));
   return Endian::fromLittleEndianToNative(value);
}


static  std::string readStringValue(ole::basic_stream& stream)
{
    int32_t string_length;
    std::string value;
    stream.read((char*)&string_length, sizeof(string_length));
    string_length = Endian::fromLittleEndianToNative(string_length);
    if(string_length > 0)
    {
        std::vector<char> buffer(string_length, 0);
        stream.read((char*)buffer.data(), string_length);
        std::u16string src((char16_t*)buffer.data());
		if (!Endian::isLittleEndian()) {
			src = Endian::u16StringLittleToBig(src);
		}
        value = Tools::fromUnicode16(src);
    }
    return value;
}

std::string ZVIUtils::readStringItem(ole::basic_stream& stream)
{
   uint16_t type(0);
   stream.read((char*)&type, sizeof(type));
   type = Endian::fromLittleEndianToNative(type);
   if(type != VT_BSTR)
   {
      std::string error = "Unexpected data type reading of compound stream. Expected string. Received:";
      error += std::to_string(type);
      throw std::runtime_error(error);
   }
   std::string value;
   return readStringValue(stream);
}

template<typename T>
static T  readTypedValue(ole::basic_stream& stream)
{
    T val(0);
    stream.read((char*)&val, sizeof(val));
    return val;
}

ZVIUtils::Variant ZVIUtils::readItem(ole::basic_stream& stream, bool skipUnusedTypes)
{
    Variant value;
    uint16_t type;
    stream.read((char*)&type, sizeof(type));
	type = Endian::fromLittleEndianToNative(type);
    uint32_t offset = 0;
    switch ((VARENUM)type)
    {
    case VT_EMPTY:
    case VT_NULL:
        break;
    case VT_I1:
        value = static_cast<int32_t>(readTypedValue<int8_t>(stream));
        break;
    case VT_UI1:
        value = static_cast<int32_t>(readTypedValue<uint8_t>(stream));
        break;
    case VT_I2:
        value = Endian::fromLittleEndianToNative(static_cast<int32_t>(readTypedValue<int16_t>(stream)));
        break;
    case VT_UI2:
        value = Endian::fromLittleEndianToNative(static_cast<int32_t>(readTypedValue<uint16_t>(stream)));
        break;
    case VT_BOOL:
        value = static_cast<bool>(Endian::fromLittleEndianToNative(readTypedValue<uint16_t>(stream)));
        break;
    case VT_I4:
        value = Endian::fromLittleEndianToNative(static_cast<int32_t>(readTypedValue<int32_t>(stream)));
        break;
    case VT_INT:
        value = Endian::fromLittleEndianToNative(static_cast<int32_t>(readTypedValue<int32_t>(stream)));
        break;
    case VT_UI4:
        value = Endian::fromLittleEndianToNative(static_cast<uint32_t>(readTypedValue<uint32_t>(stream)));
        break;
    case VT_UINT:
        value = Endian::fromLittleEndianToNative(static_cast<uint32_t>(readTypedValue<uint32_t>(stream)));
        break;
    case VT_I8:
        value = Endian::fromLittleEndianToNative(static_cast<int64_t>(readTypedValue<int64_t>(stream)));
        break;
    case VT_UI8:
        value = Endian::fromLittleEndianToNative(static_cast<uint64_t>(readTypedValue<uint64_t>(stream)));
        break;
    case VT_R4:
        value = Endian::fromLittleEndianToNative(static_cast<float>(readTypedValue<float>(stream)));
        break;
    case VT_R8:
        value = Endian::fromLittleEndianToNative(static_cast<double>(readTypedValue<double>(stream)));
        break;
    case VT_BSTR:
        value = readStringValue(stream);
        break;
    case VT_DISPATCH:
    case VT_UNKNOWN:
        offset = 16;
        break;
    case VT_ARRAY:
        stream.read((char*)&offset, 4);
        break;
    case VT_DATE:
        offset = 8;
        break;
    default:
        RAISE_RUNTIME_ERROR << "ZVIImageDriver: Unsuported item type: " << type;
    }
    if(offset>0)
    {
        stream.seek(offset, std::ios::cur);
    }
    return value;
}

ZVIUtils::StreamKeeper::StreamKeeper(ole::compound_document& doc, const std::string& path)
{
    std::vector<std::string> items;
    const size_t pos = path.find_last_of('/');
    std::string storagePath = path.substr(0, pos);
    std::string stream = path.substr(pos);
    auto storagePos = doc.find_storage(storagePath);

    if(storagePos==0)
    {
        storagePath = "/";
    }

    if(storagePos == doc.end())
    {
        RAISE_RUNTIME_ERROR << "ZVIImageDriver: Invalid storage path: " << storagePath;
    }

    m_StreamPos = storagePos->find_stream(path);
    if(m_StreamPos == storagePos->end())
    {
        RAISE_RUNTIME_ERROR << "ZVIImageDriver: Invalid stream path: " << path;
    }
}

slideio::DataType ZVIUtils::dataTypeFromPixelFormat(const ZVIPixelFormat pixelFormat)
{
    DataType dt = DataType::DT_Unknown;

    switch (pixelFormat)
    {
    case ZVIPixelFormat::PF_BGR:
    case ZVIPixelFormat::PF_BGRA:
    case ZVIPixelFormat::PF_UINT8:
        dt = DataType::DT_Byte;
        break;
    case ZVIPixelFormat::PF_BGR16:
    case ZVIPixelFormat::PF_INT16:
        dt = DataType::DT_Int16;
        break;
    case ZVIPixelFormat::PF_BGR32:
    case ZVIPixelFormat::PF_INT32:
        dt = DataType::DT_Int32;
        break;
    case ZVIPixelFormat::PF_FLOAT:
        dt = DataType::DT_Float32;
        break;
    case ZVIPixelFormat::PF_DOUBLE:
        dt = DataType::DT_Float64;
        break;
    case ZVIPixelFormat::PF_UNKNOWN:
    default:
        RAISE_RUNTIME_ERROR << "ZVIImageDriver: Invalid pixel format: " << (int)pixelFormat;
    }
    return dt;
}

int ZVIUtils::channelCountFromPixelFormat(const ZVIPixelFormat pixelFormat)
{
    int channels = 1;
    switch (pixelFormat)
    {
    case ZVIPixelFormat::PF_BGR16:
    case ZVIPixelFormat::PF_BGR32:
    case ZVIPixelFormat::PF_BGR:
        channels = 3;
        break;
    case ZVIPixelFormat::PF_BGRA:
        channels = 4;
        break;
    case ZVIPixelFormat::PF_UINT8:
    case ZVIPixelFormat::PF_INT16:
    case ZVIPixelFormat::PF_INT32:
    case ZVIPixelFormat::PF_FLOAT:
    case ZVIPixelFormat::PF_DOUBLE:
        channels = 1;
        break;
    default:
        RAISE_RUNTIME_ERROR << "ZVIImageDriver: unexpected pixel format: " << static_cast<int>(pixelFormat);
    }
    return channels;
}
