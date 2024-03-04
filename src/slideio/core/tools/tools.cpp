// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
#include "slideio/core/tools/tools.hpp"

#include <codecvt>
#include <boost/algorithm/string.hpp>
#include <numeric>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include "slideio/base/exceptions.hpp"
#if defined(WIN32)
#include <Shlwapi.h>
#else
#include <fnmatch.h>
#endif
using namespace slideio;
namespace fs = boost::filesystem;

extern "C" int wildmat(char* text, char* p);


bool Tools::matchPattern(const std::string& path, const std::string& pattern)
{
    bool ret(false);
#if defined(WIN32)
    const std::wstring wpath = Tools::toWstring(path);
    const std::wstring wpattern = Tools::toWstring(pattern);
    ret = PathMatchSpecW(wpath.c_str(), wpattern.c_str()) != 0;
#else
    std::vector<std::string> subPatterns;
    boost::algorithm::split(subPatterns, pattern, boost::is_any_of(";"), boost::algorithm::token_compress_on);
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
    target[0] = (*((uint16_t*)source)) & 0xFFF;
    uint8_t* next = source + 1;
    target[1] = (*((uint16_t*)(source + 1))) >> 4;
}

void Tools::convert12BitsTo16Bits(uint8_t* source, uint16_t* target, int targetLen)
{
    uint16_t buff[2] = {0};
    uint16_t* targetPtr = target;
    uint8_t* sourcePtr = source;
    int srcBits = 0;
    for (int ind = 0; ind < targetLen; ind += 2, targetPtr += 2, sourcePtr += 3) {
        if ((ind + 1) < targetLen) {
            convertPair12BitsTo16Bits(sourcePtr, targetPtr);
        }
        else {
            *targetPtr = (*((uint16_t*)sourcePtr)) & 0xFFF;
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

std::wstring Tools::toWstring(const std::string& string)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    std::wstring wstr = converter.from_bytes(string);
    return wstr;
}

std::string Tools::fromWstring(const std::wstring& wstring)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    std::string str = converter.to_bytes(wstring);
    return str;
}

std::string Tools::fromUnicode16(const std::u16string& u16string) {
    std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;
    std::string str = converter.to_bytes(u16string);
    str.erase(std::find(str.begin(), str.end(), '\0'), str.end());
    return str;
}

void Tools::throwIfPathNotExist(const std::string& path, const std::string label)
{
    namespace fs = boost::filesystem;
#if defined(WIN32)
    std::wstring wsPath = Tools::toWstring(path);
    boost::filesystem::path filePath(wsPath);
    if (!fs::exists(wsPath)) {
        RAISE_RUNTIME_ERROR << label << "File " << path << " does not exist";
    }
#else
    boost::filesystem::path filePath(path);
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
        const int numChannels = static_cast<int>(channels.size());
        std::vector<cv::Mat> channelRasters(numChannels);
        for (int channel = 0; channel < numChannels; ++channel) {
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
