// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/base/exceptions.hpp"
#include "slideio/base/log.hpp"
#include "zviutils.hpp"
#include "zvipixelformat.hpp"
#include <locale>

#include "slideio/core/tools/endian.hpp"
#include "slideio/core/tools/tools.hpp"
using namespace slideio;

namespace slideio
{
    namespace ZVIUtils
    {
        std::streamoff streamSize(ole::basic_stream& stream)
        {
            const std::streamoff pos = stream.pos();
            const std::streamoff size = stream.seek(0, std::ios::end);
            stream.seek(pos, std::ios::beg);
            return size;
        }

        std::streamoff bytesLeft(ole::basic_stream& stream)
        {
            const std::streamoff pos = stream.pos();
            const std::streamoff size = streamSize(stream);
            return (size > pos) ? (size - pos) : 0;
        }

        // An unchecked read at the end of a stream hands back whatever the
        // destination already held -- for a stack local, uninitialized memory --
        // which is how a stream overrun reached users as "Unsupported item
        // type: <garbage>", a different number on every run and never the cause.
        void readExactly(ole::basic_stream& stream, void* buffer, std::streamsize size)
        {
            const std::streamsize read = stream.read(static_cast<char*>(buffer), size);
            if (read != size)
            {
                RAISE_RUNTIME_ERROR << "ZVIImageDriver: unexpected end of stream at offset "
                    << static_cast<long long>(stream.pos()) << ": "
                    << static_cast<long long>(size) << " bytes requested, "
                    << static_cast<long long>(read) << " available";
            }
        }

        // POLE refuses to seek past the end of a stream and reports nothing when
        // it does, leaving the position where it was: an unchecked skip of a
        // bogus length silently continues the parse at the wrong offset.
        static void skipExactly(ole::basic_stream& stream, std::streamoff size)
        {
            if (size <= 0)
            {
                return;
            }
            if (size > bytesLeft(stream))
            {
                RAISE_RUNTIME_ERROR << "ZVIImageDriver: unexpected end of stream at offset "
                    << static_cast<long long>(stream.pos()) << ": cannot skip "
                    << static_cast<long long>(size) << " bytes, only "
                    << static_cast<long long>(bytesLeft(stream)) << " left";
            }
            stream.seek(size, std::ios::cur);
        }

        // Number of payload bytes that follow the 2-byte type token of an item.
        // For the length-prefixed types the 4-byte length prefix is consumed
        // here and its value returned.
        //
        // This is the single place where item sizes are defined. skipItem() and
        // readItem() both go through it, so the two can never disagree about
        // how many bytes an item occupies: a disagreement desynchronizes the
        // stream and makes the *next* readItem() interpret payload bytes as a
        // type token.
        static uint32_t itemPayloadSize(ole::basic_stream& stream, uint16_t type)
        {
            switch (type)
            {
            case VT_EMPTY:
            case VT_NULL:
                return 0;
            case VT_I1:
            case VT_UI1:
                return 1;
            case VT_I2:
            case VT_UI2:
            case VT_BOOL:
                return 2;
            case VT_I4:
            case VT_UI4:
            case VT_INT:
            case VT_UINT:
            case VT_R4:
            case VT_ERROR:
                return 4;
            case VT_I8:
            case VT_UI8:
            case VT_R8:
            case VT_DATE:
            case VT_CY:
                return 8;
            case VT_DISPATCH:
            case VT_UNKNOWN:
            case VT_DECIMAL:
                return 16;
            case VT_BSTR:
            case VT_BLOB:
            case VT_STORED_OBJECT:
            case VT_ARRAY:
            case 0x003F:
                // 0x3F has no VARENUM name. Bio-Formats' ZeissZVIReader
                // getNextTag() steps over it as a 32-bit length prefixed blob,
                // like VT_BLOB, which is the only description of it we have.
                {
                    uint32_t length = 0;
                    readExactly(stream, &length, sizeof(length));
                    return Endian::fromLittleEndianToNative(length);
                }
            case VT_STREAM:
                {
                    // Unlike the types above, VT_STREAM is prefixed with a
                    // 16-bit length (matches Bio-Formats' ZeissZVIReader).
                    uint16_t length = 0;
                    readExactly(stream, &length, sizeof(length));
                    return Endian::fromLittleEndianToNative(length);
                }
            default:
                break;
            }
            // Guessing a size here would silently desynchronize the stream, so
            // the unknown type is reported at the offset it was read from.
            RAISE_RUNTIME_ERROR << "ZVIImageDriver: Unsupported item type: " << type
                << " at stream offset " << static_cast<long long>(stream.pos()) - 2;
        }
    }
}

void ZVIUtils::skipItem(ole::basic_stream& stream)
{
    uint16_t type = 0;
    readExactly(stream, &type, sizeof(type));
    type = Endian::fromLittleEndianToNative(type);
    skipExactly(stream, itemPayloadSize(stream, type));
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
   readExactly(stream, &type, sizeof(type));
   type = Endian::fromLittleEndianToNative(type);
   if(type != VT_I4 && type != VT_INT)
   {
      std::string error =
         "Unexpected data type reading of compound stream. Expected integer. Received:";
      error += std::to_string(type);
      throw std::runtime_error(error);
   }
   int32_t value = 0;
   readExactly(stream, &value, sizeof(value));
   return Endian::fromLittleEndianToNative(value);
}

double ZVIUtils::readDoubleItem(ole::basic_stream& stream)
{
   uint16_t type(0);
   readExactly(stream, &type, sizeof(type));
   type = Endian::fromLittleEndianToNative(type);
   if(type != VT_R8)
   {
      std::string error =
         "Unexpected data type reading of compound stream. Expected VT_R8. Received:";
      error += std::to_string(type);
      throw std::runtime_error(error);
   }
   double value = 0.;
   readExactly(stream, &value, sizeof(value));
   return Endian::fromLittleEndianToNative(value);
}


static  std::string readStringValue(ole::basic_stream& stream)
{
    int32_t string_length = 0;
    std::string value;
    ZVIUtils::readExactly(stream, &string_length, sizeof(string_length));
    string_length = Endian::fromLittleEndianToNative(string_length);
    if(string_length > 0)
    {
        std::vector<char> buffer(string_length, 0);
        ZVIUtils::readExactly(stream, buffer.data(), string_length);
        // The payload is not guaranteed to be NUL terminated, so the length has
        // to bound the string: constructing it from the raw pointer alone read
        // past the end of the buffer whenever the terminator was missing.
        std::u16string src(reinterpret_cast<const char16_t*>(buffer.data()),
                           static_cast<size_t>(string_length) / sizeof(char16_t));
        const size_t terminator = src.find(char16_t(0));
        if (terminator != std::u16string::npos) {
            src.resize(terminator);
        }
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
   readExactly(stream, &type, sizeof(type));
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
    ZVIUtils::readExactly(stream, &val, sizeof(val));
    return val;
}

ZVIUtils::Variant ZVIUtils::readItem(ole::basic_stream& stream, bool skipUnusedTypes)
{
    Variant value;
    uint16_t type = 0;
    readExactly(stream, &type, sizeof(type));
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
    default:
        // Types the driver does not decode into a Variant (VT_DATE, VT_BLOB,
        // VT_DISPATCH, ...) still have to be stepped over exactly. Truly
        // unknown types are rejected by itemPayloadSize().
        offset = itemPayloadSize(stream, type);
        break;
    }
    skipExactly(stream, offset);
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

std::vector<ZVIUtils::ZviTagEntry> ZVIUtils::readAllTags(
    ole::basic_stream& stream, bool hasClsidHeader)
{
    if (hasClsidHeader) {
        // 128-bit CLSID — raw, not a typed token.
        stream.seek(16, std::ios::cur);
    }
    const int32_t version = readIntItem(stream);
    (void)version; // not used; spec value is informational.
    const int32_t count = readIntItem(stream);
    if (count < 0 || count > 100000) {
        return {};
    }
    std::vector<ZviTagEntry> entries;
    entries.reserve(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        // {Count} is not always the number of tags the stream actually holds;
        // Bio-Formats' ZeissZVIReader.parseTags() bounds its loop the same way.
        if (bytesLeft(stream) < 2) {
            SLIDEIO_LOG(WARNING) << "ZVIImageDriver: tag stream ends after " << i
                << " of " << count << " declared tags";
            break;
        }
        Variant value;
        int32_t id = 0;
        try {
            value = readItem(stream);
            id = readIntItem(stream);
            skipItem(stream); // {Attribute} — no longer used per spec.
        }
        catch (const std::exception& e) {
            // An unreadable tag truncates the metadata; it must not cost the
            // caller the tags read before it, nor the file its scenes.
            SLIDEIO_LOG(WARNING) << "ZVIImageDriver: stopped reading tags after " << i
                << " of " << count << ": " << e.what();
            break;
        }
        if (value.index() == 0) {
            continue; // std::monostate: VT_EMPTY/VT_NULL/blob/object — drop.
        }
        entries.push_back(ZviTagEntry{ id, std::move(value) });
    }
    return entries;
}
