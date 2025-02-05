// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
#include "slideio/core/tools/tools.hpp"

#include <codecvt>
#include <numeric>
#include "slideio/base/exceptions.hpp"
#include <filesystem>
#if defined(WIN32)
#include <Shlwapi.h>
#else
#include <fnmatch.h>
#endif
#include <string>
#include <stdexcept>
#include <unicode/unistr.h>
using namespace slideio;
namespace fs = std::filesystem;

extern "C" {
    #include "wildmat.h"
}

std::vector<std::string> Tools::split(const std::string& val, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(val);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    // Check for trailing delimiter
    if (!val.empty() && val.back() == delimiter) {
        tokens.push_back("");
    }
    return tokens;
}

bool Tools::matchPattern(const std::string& path, const std::string& pattern)
{
    bool ret(false);
#if defined(WIN32)
    const std::wstring wpath = Tools::toWstring(path);
    const std::wstring wpattern = Tools::toWstring(pattern);
    ret = PathMatchSpecW(wpath.c_str(), wpattern.c_str()) != 0;
#else
    std::vector<std::string> subPatterns = split(pattern, ';');
    for(const auto& sub_pattern : subPatterns)
    {
        ret = wildmat(const_cast<char*>(path.c_str()),const_cast<char*>(sub_pattern.c_str()));
        if(ret){
            break;
        }
    }
#endif
    return ret;
}

inline void convertPair12BitsTo16Bits(uint8_t* source, uint16_t* target)
{
    #ifdef _MSC_VER
        target[0] = (*((uint16_t*)source)) & 0xFFF;
        target[1] = (*((uint16_t*)(source + 1))) >> 4;
    #else
        uint16_t value1, value2;
        std::memcpy(&value1, source, sizeof(uint16_t));
        std::memcpy(&value2, source + 1, sizeof(uint16_t));
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        target[0] = value1 & 0xFFF;
        target[1] = value2 >> 4;
    #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        target[0] = ntohs(value1) & 0xFFF;
        target[1] = ntohs(value2) >> 4;
    #else
    #error "Unsupported byte order"
    #endif
    #endif
}

void Tools::convert12BitsTo16Bits(uint8_t* source, uint16_t* target, int targetLen)
{
    uint16_t* targetPtr = target;
    uint8_t* sourcePtr = source;
    for (int ind = 0; ind < targetLen; ind += 2, targetPtr += 2, sourcePtr += 3) {
        if ((ind + 1) < targetLen) {
            convertPair12BitsTo16Bits(sourcePtr, targetPtr);
        }
        else {
#ifdef _MSC_VER
            *targetPtr = (*((uint16_t*)sourcePtr)) & 0xFFF;
#else
            uint16_t value;
            std::memcpy(&value, sourcePtr, sizeof(uint16_t));
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
            *targetPtr = value & 0xFFF;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            *targetPtr = ntohs(value) & 0xFFF;
#else
#error "Unsupported byte order"
#endif
#endif        
        }
    }
}

void slideio::Tools::scaleRect(const cv::Rect& srcRect, const cv::Size& newSize, cv::Rect& trgRect)
{
    double scaleX = static_cast<double>(newSize.width) / static_cast<double>(srcRect.width);
    double scaleY = static_cast<double>(newSize.height) / static_cast<double>(srcRect.height);
    trgRect.x = static_cast<int>(std::floor(static_cast<double>(srcRect.x) * scaleX));
    trgRect.y = static_cast<int>(std::floor(static_cast<double>(srcRect.y) * scaleY));
    trgRect.width = newSize.width;
    trgRect.height = newSize.height;
}

void slideio::Tools::scaleRect(const cv::Rect& srcRect, double scaleX, double scaleY, cv::Rect& trgRect)
{
    trgRect.x = static_cast<int>(std::floor(static_cast<double>(srcRect.x) * scaleX));
    trgRect.y = static_cast<int>(std::floor(static_cast<double>(srcRect.y) * scaleY));
    int xn = srcRect.x + srcRect.width;
    int yn = srcRect.y + srcRect.height;
    int dxn = static_cast<int>(std::ceil(static_cast<double>(xn) * scaleX));
    int dyn = static_cast<int>(std::ceil(static_cast<double>(yn) * scaleY));
    trgRect.width = dxn - trgRect.x;
    trgRect.height = dyn - trgRect.y;
}

#if defined(WIN32)
  std::wstring Tools::toWstring(const std::string& utf8Str)
{
      if (utf8Str.empty()) {
          return std::wstring();
      }
	  const int bytes = static_cast<int>(utf8Str.length());
      const int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8Str.c_str(), bytes, nullptr, 0);
      if (wlen == 0) {
          DWORD error = GetLastError();
          if(error== ERROR_NO_UNICODE_TRANSLATION) {
              RAISE_RUNTIME_ERROR << "Unrecognized UTF-8 charachters: " << utf8Str;
          }
          return std::wstring();
      }

      std::wstring wstr(wlen, L'\0');
      MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), bytes, wstr.data(), wlen);
      
      return wstr;
  }
#endif


std::string Tools::fromUnicode16(const std::u16string& u16string)
{
    if (u16string.empty()) return std::string();
    icu::UnicodeString unicode_str(reinterpret_cast<const UChar*>(u16string.data()), (int)u16string.length());
    std::string utf8_string;
    unicode_str.toUTF8String(utf8_string);
    return utf8_string;
}

void Tools::throwIfPathNotExist(const std::string& path, const std::string label)
{
#if defined(WIN32)
    std::wstring wsPath = Tools::toWstring(path);
    fs::path filePath(wsPath);
    if (!fs::exists(wsPath)) {
        RAISE_RUNTIME_ERROR << label << "File " << path << " does not exist";
    }
#else
    fs::path filePath(path);
    if (!fs::exists(filePath)) {
        RAISE_RUNTIME_ERROR << label << " File " << path << " does not exist";
    }
#endif
}

std::list<std::string> Tools::findFilesWithExtension(const std::string& directory, const std::string& extension)
{
    std::list<std::string> filePaths;

    if (!fs::exists(directory) || !fs::is_directory(directory)) {
        std::cerr << "Invalid directory path or not a directory." << std::endl;
        return filePaths;
    }

    for (fs::recursive_directory_iterator it(directory); it != fs::recursive_directory_iterator(); ++it) {
        if (fs::is_regular_file(*it) && it->path().extension() == extension) {
            filePaths.push_back(fs::canonical(*it).string());
        }
    }
    return filePaths;
}

void Tools::extractChannels(const cv::Mat& sourceRaster, const std::vector<int>& channels, cv::OutputArray output)
{
    if (channels.empty()) {
        sourceRaster.copyTo(output);
    }
    else {
        const int rasterChannelCount = sourceRaster.channels();
        const int numChannels = static_cast<int>(channels.size());
        std::vector<cv::Mat> channelRasters(numChannels);
        for (int channel = 0; channel < numChannels; ++channel) {
            if(channel >= rasterChannelCount) {
                RAISE_RUNTIME_ERROR << "Attempt to extract channel " << channel << " from " << rasterChannelCount << " channels.";
            }
            cv::extractChannel(sourceRaster, channelRasters[channel], channels[channel]);
        }
        cv::merge(channelRasters, output);
    }
}

FILE* Tools::openFile(const std::string& filePath, const char* mode)
{
#if defined(WIN32)
    std::wstring wfilePath = Tools::toWstring(filePath);
    std::wstring wmode = Tools::toWstring(mode);
    return _wfopen(wfilePath.c_str(), wmode.c_str());
#else
    return fopen(filePath.c_str(), mode);
#endif
}

uint64_t Tools::getFilePos(FILE* file)
{
#if defined(WIN32)
    return _ftelli64(file);
#elif __APPLE__
    return ftello(file);
#else
    return ftello64(file);
#endif
}

int Tools::setFilePos(FILE* file, uint64_t pos, int origin)
{
#if defined(WIN32)
    return _fseeki64(file, pos, origin);
#elif __APPLE__
    return fseeko(file, pos, origin);
#define FTELL64 ftello
#else
    return fseeko64(file, pos, origin);
#endif
}

uint64_t Tools::getFileSize(FILE* file)
{
    uint64_t pos = Tools::getFilePos(file);
    Tools::setFilePos(file, 0, SEEK_END);
    uint64_t size = Tools::getFilePos(file);
    Tools::setFilePos(file, pos, SEEK_SET);
    return size;
}

int Tools::dataTypeSize(slideio::DataType dt)
{
    switch (dt)
    {
    case DataType::DT_Byte:
    case DataType::DT_Int8:
        return 1;
    case DataType::DT_UInt16:
    case DataType::DT_Int16:
    case DataType::DT_Float16:
        return 2;
    case DataType::DT_Int32:
    case DataType::DT_Float32:
        return 4;
    case DataType::DT_Float64:
        return 8;
    case DataType::DT_Unknown:
    case DataType::DT_None:
        break;
    }
    RAISE_RUNTIME_ERROR << "Unknown data type: " << (int)dt;
}

void Tools::replaceAll(std::string& str, const std::string& from, const std::string& to)
{
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}
