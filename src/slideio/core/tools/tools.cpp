// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
#include "slideio/core/tools/tools.hpp"

#include <codecvt>
#include <numeric>
#include "slideio/core/exceptions.hpp"
#include <filesystem>
#include <random>
#if defined(WIN32)
#include <Shlwapi.h>
#else
#include <fnmatch.h>
#endif
#include <string>
#include <stdexcept>
#include <cctype>
#include <unicode/unistr.h>
//#include <arpa/inet.h>

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

std::string Tools::randomUUID() {
    static const char* hex = "0123456789abcdef";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 15);
    std::string s(36, '\0');
    int positions[] = { 8, 13, 18, 23 };
    int pIndex = 0;
    for (int i = 0; i < 36; ++i) {
        if (pIndex < 4 && i == positions[pIndex]) { 
            s[i] = '-'; 
            ++pIndex; 
            continue; 
        }
        int val = dist(gen);
        s[i] = hex[val];
    }
    return "urn:uuid:" + s;
}

void Tools::resize(cv::InputArray src, cv::OutputArray dst, cv::Size dsize, int interpolation) {
    // cv::resize does not support the CV_8S (signed 8-bit) depth, so handle it here.
    if (src.depth() == CV_8S) {
        const cv::Mat srcMat = src.getMat();
        const int channels = srcMat.channels();
        if (interpolation == cv::INTER_NEAREST) {
            // Nearest-neighbor only copies pixel values, so the signed/unsigned
            // interpretation is irrelevant. Reinterpret the bytes as CV_8U,
            // resize, then reinterpret the result back to CV_8S (exact, no promotion).
            const cv::Mat srcU(srcMat.rows, srcMat.cols,
                               CV_MAKETYPE(CV_8U, channels), srcMat.data, srcMat.step);
            cv::Mat resizedU;
            cv::resize(srcU, resizedU, dsize, 0, 0, interpolation);
            const cv::Mat resizedS(resizedU.rows, resizedU.cols,
                                   CV_MAKETYPE(CV_8S, channels), resizedU.data, resizedU.step);
            resizedS.copyTo(dst);
        }
        else {
            // Interpolating methods need signed arithmetic. Promote to CV_16S
            // (lossless, sign-preserving), resize, then convert back to CV_8S
            // with saturation.
            cv::Mat tmp;
            srcMat.convertTo(tmp, CV_16S);
            cv::Mat resized;
            cv::resize(tmp, resized, dsize, 0, 0, interpolation);
            resized.convertTo(dst, CV_8S);
        }
        return;
    }
    // cv::resize does not support the CV_32S (signed 32-bit) depth either.
    if (src.depth() == CV_32S) {
        const cv::Mat srcMat = src.getMat();
        const int channels = srcMat.channels();
        if (interpolation == cv::INTER_NEAREST) {
            // Nearest-neighbor copies whole pixels, so reinterpret the 4-byte
            // elements as CV_32F (bit-preserving), resize, then reinterpret back
            // to CV_32S. Exact for the full int32 range.
            const cv::Mat srcF(srcMat.rows, srcMat.cols,
                               CV_MAKETYPE(CV_32F, channels), srcMat.data, srcMat.step);
            cv::Mat resizedF;
            cv::resize(srcF, resizedF, dsize, 0, 0, interpolation);
            const cv::Mat resizedS(resizedF.rows, resizedF.cols,
                                   CV_MAKETYPE(CV_32S, channels), resizedF.data, resizedF.step);
            resizedS.copyTo(dst);
        }
        else {
            // Interpolating methods need arithmetic. CV_64F is the only float
            // depth that represents the full int32 range exactly (CV_32F's
            // 24-bit mantissa loses precision above 2^24), but the promotion
            // must NOT go through cv::Mat::convertTo: OpenCV's CV_32S -> CV_64F
            // kernel routes the value via float, so convertTo alone rounds
            // -123456789 to -123456792 before any resizing happens. Widen
            // int32 -> double directly instead, which is lossless by the C++
            // conversion rules, then resize and convert back to CV_32S with
            // saturation (the CV_64F -> CV_32S direction is exact).
            cv::Mat tmp(srcMat.rows, srcMat.cols, CV_MAKETYPE(CV_64F, channels));
            const int valuesPerRow = srcMat.cols * channels;
            for (int y = 0; y < srcMat.rows; ++y) {
                const int32_t* sourceRow = srcMat.ptr<int32_t>(y);
                double* targetRow = tmp.ptr<double>(y);
                for (int i = 0; i < valuesPerRow; ++i) {
                    targetRow[i] = static_cast<double>(sourceRow[i]);
                }
            }
            cv::Mat resized;
            cv::resize(tmp, resized, dsize, 0, 0, interpolation);
            resized.convertTo(dst, CV_32S);
        }
        return;
    }
    cv::resize(src, dst, dsize, 0, 0, interpolation);
}

#if !defined(WIN32)
namespace
{
    // Lower-cases the ASCII letters and nothing else. Deliberately not
    // std::tolower: that consults the global C locale, which a host application
    // is free to change, and the answer to "can this driver open this file"
    // must not depend on that. Bytes outside A-Z are left exactly as they are,
    // so a UTF-8 path keeps its multi-byte sequences intact -- folding those
    // correctly would need real Unicode case mapping, and every pattern this is
    // used with is a plain ASCII extension.
    std::string asciiToLower(const std::string& value)
    {
        std::string lowered(value);
        for (char& symbol : lowered) {
            if (symbol >= 'A' && symbol <= 'Z') {
                symbol = static_cast<char>(symbol - 'A' + 'a');
            }
        }
        return lowered;
    }
}
#endif

bool Tools::matchPattern(const std::string& path, const std::string& pattern)
{
    bool ret(false);
#if defined(WIN32)
    const std::wstring wpath = Tools::toWstring(path);
    const std::wstring wpattern = Tools::toWstring(pattern);
    ret = PathMatchSpecW(wpath.c_str(), wpattern.c_str()) != 0;
#else
    // wildmat compares case-sensitively; PathMatchSpecW on the branch above does
    // not. This function has one caller, ImageDriver::canOpenFile, and that is
    // what ImageDriverManager uses to choose a driver -- so with a case-sensitive
    // comparison a slide named SCAN.OME.TIFF opens on Windows and reports
    // "Cannot find driver" on Linux and macOS. Fold both sides instead, which
    // matches what Windows has always done rather than changing it.
    const std::string loweredPath = asciiToLower(path);
    const std::string loweredPattern = asciiToLower(pattern);
    std::vector<std::string> subPatterns = split(loweredPattern, ';');
    for(const auto& sub_pattern : subPatterns)
    {
        ret = wildmat(const_cast<char*>(loweredPath.c_str()),const_cast<char*>(sub_pattern.c_str()));
        if(ret){
            break;
        }
    }
#endif
    return ret;
}

bool Tools::isConsecutiveFromZero(const std::vector<int>& vec, int size) {
    if (vec.size() != static_cast<size_t>(size)) {
        return false;
	}
    for (size_t i = 0; i < vec.size(); ++i) {
        if (vec[i] != static_cast<int>(i)) {
            return false;
        }
    }
    return true;
}


void Tools::convert12BitsTo16Bits(const uint8_t* source, uint16_t* target, int targetLen) {
    if (!source || !target || targetLen <= 0)
        RAISE_RUNTIME_ERROR << "Tools::convert12BitsTo16Bits: Invalid parameters"
        << "source:" << (source != nullptr)
        << " target:" << (target != nullptr)
        << " targetLen:" << targetLen;
    int index = 0;
    while (index < targetLen) {
        // Extract two 12-bit numbers from 3 bytes
        const uint16_t first = (source[0] << 4) | (source[1] >> 4);
        target[index++] = first;
        if (index < targetLen) {
            const uint16_t second = ((source[1] & 0x0F) << 8) | source[2];
            target[index++] = second;
        }
        else {
            break;
        }
        source += 3;
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
          RAISE_RUNTIME_ERROR << "UTF-8 to wide string conversion failed (error " << error << "): " << utf8Str;
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
