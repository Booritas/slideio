// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
#include "slideio/core/tools/tools.hpp"

#include <cstdio>
#include <algorithm>
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

std::optional<double> Tools::parseIso8601(const std::string& text)
{
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    int consumed = 0;
    if (std::sscanf(text.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d%n",
                    &year, &month, &day, &hour, &minute, &second, &consumed) != 6) {
        return std::nullopt;
    }
    // Floors as well as ceilings: %2d consumes a sign, so "T-1:45:48" would
    // otherwise parse to an hour before midnight of the stated day.
    if (year < 1 || month < 1 || month > 12 || day < 1 || day > 31
        || hour < 0 || hour > 23 || minute < 0 || minute > 59
        || second < 0 || second > 60) {
        return std::nullopt;
    }
    static const int monthLengths[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    if (day > monthLengths[month - 1] + ((leap && month == 2) ? 1 : 0)) {
        return std::nullopt;
    }

    // Days from the civil date, after Howard Hinnant: no timegm, which is not
    // portable, and no mktime, which would drag in the local timezone.
    int y = year;
    y -= month <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const int64_t days = static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;

    double epoch = static_cast<double>(days) * 86400.0
        + hour * 3600.0 + minute * 60.0 + second;

    const char* rest = text.c_str() + consumed;
    if (*rest == '.') {
        // Kept rather than skipped: CZI states sub-block times to seven places
        // and neighbouring planes differ by tens of milliseconds.
        // Read digit by digit rather than with strtod, which honours the
        // decimal point of the current C locale: on a comma-decimal host whose
        // application has called setlocale(LC_ALL, "") -- Qt does -- strtod
        // would return 0 here and every time would collapse to its whole second.
        ++rest;
        int64_t fraction = 0;
        double denominator = 1.;
        while (*rest >= '0' && *rest <= '9') {
            if (denominator < 1.e18) {
                fraction = fraction * 10 + (*rest - '0');
                denominator *= 10.;
            }
            ++rest;
        }
        epoch += static_cast<double>(fraction) / denominator;
    }
    if (*rest == 'Z' || *rest == 'z') {
        ++rest;
    }
    else if (*rest == '+' || *rest == '-') {
        int offsetHours = 0, offsetMinutes = 0;
        // An offset that cannot be read is refused rather than assumed to be
        // UTC: "+0200" without the colon is not ISO 8601, and dropping it
        // silently is a two hour error in something meant to name one instant.
        if (std::sscanf(rest + 1, "%2d:%2d", &offsetHours, &offsetMinutes) != 2
            || offsetHours < 0 || offsetHours > 14
            || offsetMinutes < 0 || offsetMinutes > 59) {
            return std::nullopt;
        }
        const double offset = offsetHours * 3600.0 + offsetMinutes * 60.0;
        epoch += (*rest == '+') ? -offset : offset;
    }
    return epoch;
}

std::optional<int64_t> Tools::parseTiffDateTime(const std::string& text)
{
    // "YYYY:MM:DD HH:MM:SS" is exactly nineteen characters and TIFF fixes the
    // length, so a writer with nothing to say leaves spaces behind rather than
    // omitting the tag. Rebuilt into ISO 8601 rather than parsed again here:
    // parseIso8601 already validates the calendar and the ranges.
    if (text.size() < 19) {
        return std::nullopt;
    }
    if (text[4] != ':' || text[7] != ':' || text[10] != ' '
        || text[13] != ':' || text[16] != ':') {
        return std::nullopt;
    }
    std::string iso = text.substr(0, 4) + "-" + text.substr(5, 2) + "-" + text.substr(8, 2)
        + "T" + text.substr(11, 2) + ":" + text.substr(14, 2) + ":" + text.substr(17, 2) + "Z";
    const auto epoch = parseIso8601(iso);
    if (!epoch) {
        return std::nullopt;
    }
    // Whole seconds. A TIFF DateTime carries no fraction, so nothing is lost.
    return static_cast<int64_t>(std::floor(*epoch));
}

namespace
{
    bool dicomAllDigits(const std::string& text)
    {
        if (text.empty()) {
            return false;
        }
        for (const char c : text) {
            if (c < '0' || c > '9') {
                return false;
            }
        }
        return true;
    }

    std::string dicomWithoutSeparators(std::string text)
    {
        text.erase(std::remove_if(text.begin(), text.end(),
                                  [](const char c) { return c == '.' || c == ':'; }),
                   text.end());
        return text;
    }
}

std::optional<double> Tools::parseDicomDateTime(const std::string& date,
                                                const std::string& time,
                                                const std::string& zoneOffset)
{
    std::string day = date;
    std::string clock = time;
    // A DT value states both halves in one string. Split it before the
    // separators are stripped, so a dotted DA of more than eight characters --
    // "1994.10.16" -- is not mistaken for one.
    if (clock.empty() && day.size() > 8 && dicomAllDigits(day.substr(0, 8))) {
        clock = day.substr(8);
        day = day.substr(0, 8);
    }
    day = dicomWithoutSeparators(day);
    if (day.size() != 8 || !dicomAllDigits(day)) {
        return std::nullopt;
    }

    std::string offset;
    const size_t offsetAt = clock.find_first_of("+-");
    if (offsetAt != std::string::npos) {
        offset = clock.substr(offsetAt);
        clock = clock.substr(0, offsetAt);
    }
    else {
        // Nothing in the value, so the instance's offset applies. Both halves
        // of a plane time -- an acquisition DT and a content DA/TM pair -- end
        // up on one clock this way, and their difference means what it says.
        offset = zoneOffset;
    }
    clock.erase(std::remove(clock.begin(), clock.end(), ':'), clock.end());

    std::string fraction;
    const size_t dot = clock.find('.');
    if (dot != std::string::npos) {
        fraction = clock.substr(dot);
        clock = clock.substr(0, dot);
        if (fraction.size() < 2 || !dicomAllDigits(fraction.substr(1))) {
            return std::nullopt;
        }
    }
    // A TM may stop after the hour or after the minute.
    if (clock.size() < 2 || clock.size() > 6 || (clock.size() % 2) != 0 || !dicomAllDigits(clock)) {
        return std::nullopt;
    }
    clock.append(6 - clock.size(), '0');

    std::string iso = day.substr(0, 4) + "-" + day.substr(4, 2) + "-" + day.substr(6, 2)
        + "T" + clock.substr(0, 2) + ":" + clock.substr(2, 2) + ":" + clock.substr(4, 2)
        + fraction;
    if (offset.empty()) {
        iso += "Z";
    }
    else {
        // DICOM writes the offset as &ZZXX, which ISO 8601 spells with a colon.
        if (offset[0] != '+' && offset[0] != '-') {
            return std::nullopt;
        }
        const std::string digits = dicomWithoutSeparators(offset.substr(1));
        if (digits.size() != 4 || !dicomAllDigits(digits)) {
            return std::nullopt;
        }
        iso += offset[0];
        iso += digits.substr(0, 2);
        iso += ":";
        iso += digits.substr(2, 2);
    }
    return Tools::parseIso8601(iso);
}
