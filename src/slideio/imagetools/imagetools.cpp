// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/imagetools/imagetools.hpp"

#include "slideio/base/exceptions.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include "slideio/imagetools/smalltiffwrapper.hpp"
#include "slideio/imagetools/fiwrapper.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/imagetools/SmallImage.hpp"
#include "slideio/base/log.hpp"
#include <FreeImage.h>
#include <numeric>
#include <opencv2/imgproc.hpp>
#include <vector>
#include <fstream>
#include <cmath>

using namespace slideio;

namespace
{
    bool isTiff(const std::string& filePath) {
        FREE_IMAGE_FORMAT fiFormat = FIF_UNKNOWN;
#if defined(WIN32)
        std::wstring wsPath = Tools::toWstring(filePath);
        fiFormat = FreeImage_GetFileTypeU(wsPath.c_str(), 0);
        if (fiFormat == FIF_UNKNOWN) {
            fiFormat = FreeImage_GetFIFFromFilenameU(wsPath.c_str());
        }
#else
        if (readOnly && !std::filesystem::exists(filePath)) {
            RAISE_RUNTIME_ERROR << "FIWrapper: File " << filePath << " does not exist";
        }
        fiFormat = FreeImage_GetFileType(filePath.c_str(), 0);
        if (fiFormat == FIF_UNKNOWN) {
            fiFormat = FreeImage_GetFIFFromFilename(filePath.c_str());
        }
#endif
        return fiFormat == FIF_TIFF;
    }

}

double ImageTools::computeSimilarity(const cv::Mat& leftM, const cv::Mat& rightM, bool ignoreTypes)
{
    double similarity = 0;
    const cv::Size leftSize = leftM.size();
    const cv::Size rightSize = rightM.size();
    if(leftSize != rightSize)
    {
        RAISE_RUNTIME_ERROR << "Image sizes for comparison shall be equal. Left image: (" << leftSize.width << "," << leftSize.height
            << "), Right image: (" << rightSize.width << "," << rightSize.height << ")";
    }
    if(leftM.channels() != rightM.channels())
    {
        RAISE_RUNTIME_ERROR << "Number of image channesl for comparison shall be equal. Left image: " << leftM.channels()
            << ". Right image:" << rightM.channels() << ".";
    }
    int dtLeft = leftM.type();
    int dtRight = rightM.type();
    if(!ignoreTypes && dtLeft!=dtRight)
    {
        RAISE_RUNTIME_ERROR << "Image types for comparison shall be equal. Left image: " << dtLeft
            << ". Right image:" << dtRight << ".";
    }
    // convert to 8bit images
    cv::Mat oneChannelLeftM = leftM.reshape(1);
    cv::Mat oneChannelRightM = rightM.reshape(1);
    double minLeft(0), maxLeft(0), minRight(0), maxRight(0);
    cv::minMaxLoc(oneChannelLeftM, &minLeft, &maxLeft);
    cv::minMaxLoc(oneChannelRightM, &minRight, &maxRight);
    double minVal = std::min(minLeft, minRight);
    double maxVal = std::max(maxLeft, maxRight);
    double absVal = maxVal;
    absVal -= minVal;
    double alpha = 255. / absVal;
    double beta = -minVal * alpha;

    cv::Mat left, right;
    leftM.convertTo(left, CV_MAKE_TYPE(CV_8U, left.channels()), alpha, beta);
    rightM.convertTo(right, CV_MAKE_TYPE(CV_8U, right.channels()), alpha, beta);

    cv::Rect rectWindow(0, 0, 30, 30);
    const int width = left.size[1];
    const int height = left.size[0];
    std::vector<double> scores;
    cv::Rect image(0, 0, width, height);
    const int binCount = 10;

    for (int y = 0; y < height; y += rectWindow.height)
    {
        for (int x = 0; x < width; x += rectWindow.width)
        {
            cv::Rect rectWnd2(rectWindow);
            rectWnd2.x = x;
            rectWnd2.y = y;
            cv::Rect rectRoi = rectWnd2 & image;
            cv::Mat leftRoi = left(rectRoi);
            cv::Mat rightRoi = right(rectRoi);
            double score = compareHistograms(leftRoi, rightRoi, binCount);
            scores.push_back(score);
        }
    }
    similarity = std::accumulate(scores.begin(), scores.end(), 0.0) / scores.size();
    return similarity;
}

double ImageTools::computeSimilarity2(const cv::Mat& leftM, const cv::Mat& rightM)
{
    double similarity = 0;
    const cv::Size leftSize = leftM.size();
    const cv::Size rightSize = rightM.size();
    if (leftSize != rightSize)
    {
        RAISE_RUNTIME_ERROR << "Image sizes for comparison shall be equal. Left image: (" << leftSize.width << "," << leftSize.height
            << "), Right image: (" << rightSize.width << "," << rightSize.height << ")";
    }
    if (leftM.channels() != rightM.channels())
    {
        RAISE_RUNTIME_ERROR << "Number of image channesl for comparison shall be equal. Left image: " << leftM.channels()
            << ". Right image:" << rightM.channels() << ".";
    }

    const DataType dtpLeft = CVTools::fromOpencvType(leftM.type() & CV_MAT_DEPTH_MASK);
    const DataType dtpRight = CVTools::fromOpencvType(rightM.type() & CV_MAT_DEPTH_MASK);
    if (dtpLeft != dtpRight)
    {
        RAISE_RUNTIME_ERROR << "Data types for comparison shall be equal. Left image: " << dtpLeft
            << ". Right image:" << dtpRight << ".";
    }

    int dtLeft = leftM.type();
    int dtRight = rightM.type();
    if (dtLeft != dtRight)
    {
        RAISE_RUNTIME_ERROR << "Image types for comparison shall be equal. Left image: " << dtLeft
            << ". Right image:" << dtRight << ".";
    }

    double maxVal = 255;
    {
        const int channelCount = leftM.channels();
        std::vector<double> minLeft(channelCount), maxLeft(channelCount), minRight(channelCount), maxRight(channelCount);
        cv::minMaxLoc(leftM, minLeft.data(), maxLeft.data());
        cv::minMaxLoc(rightM, minRight.data(), maxRight.data());
        auto maxElementL = std::max_element(maxLeft.begin(), maxLeft.end());
        auto maxElementR = std::max_element(maxRight.begin(), maxRight.end());
        maxVal = std::max(*maxElementL, *maxElementR);
    }

    cv::Mat diff;
    cv::absdiff(leftM, rightM, diff);
    cv::Mat diffd;
    diff.convertTo(diffd, CV_MAKE_TYPE(CV_32F, diff.channels()));
    diffd /= maxVal;
    cv::pow(diffd, 1.5, diffd);
    cv::Scalar sums = cv::sum(diffd);
    auto sum = cv::sum(sums);
    double sumVal = sum[0];
    similarity = 1.0 - sumVal / (leftSize.width * leftSize.height);
    return similarity;
}

double ImageTools::compareHistograms(const cv::Mat& left, const cv::Mat& right, int binCount)
{
    double similarity = 0;

    const int channelCount = left.channels();
    std::vector<int> channels(channelCount);

    std::vector<float*> ranges(channelCount);
    std::vector<int> histSizes(channelCount);
    std::vector<float> rangeVals(channelCount * 2);
    float minVal = 0;
    float maxVal = 256;

    for (int channel = 0; channel < channelCount; ++channel) {
        channels[channel] = channel;
        histSizes[channel] = binCount;
        const int channel2 = channel * 2;
        rangeVals[channel2] = (float)minVal;
        rangeVals[channel2 + 1] = (float)maxVal;
        ranges[channel] = &rangeVals[channel2];
    }

    cv::Mat histLeft, histRight;
    cv::Mat mask;

    cv::calcHist(&left, 1, channels.data(), mask, histLeft,
        channelCount, histSizes.data(), (const float**)ranges.data());

    cv::calcHist(&right, 1, channels.data(), mask, histRight,
        channelCount, histSizes.data(), (const float**)ranges.data());

    similarity = cv::compareHist(histLeft, histRight, cv::HISTCMP_CORREL);

    return similarity;
}


std::shared_ptr<SmallImage> ImageTools::openSmallImage(const std::string& filePath) {
    if (isTiff(filePath)) {
        return std::static_pointer_cast<SmallImage>(std::make_shared<SmallTiffWrapper>(filePath));
    } else {
        return std::static_pointer_cast<SmallImage>(std::make_shared<FIWrapper>(filePath));
    }
}

void ImageTools::readBitmap(const std::string& path, cv::OutputArray output) {
    // Open the BMP file
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        RAISE_RUNTIME_ERROR << "ImageTools::readBitmap: Cannot open file " << path;
    }

    // Read BMP file header (14 bytes)
    struct BMPFileHeader {
        uint16_t fileType;      // File type, should be 0x4D42 ('BM')
        uint32_t fileSize;      // Size of the file in bytes
        uint16_t reserved1;     // Reserved, should be 0
        uint16_t reserved2;     // Reserved, should be 0
        uint32_t offsetData;    // Start position of pixel data
    };

    // Read BMP info header (40 bytes for BITMAPINFOHEADER)
    struct BMPInfoHeader {
        uint32_t headerSize;        // Size of this header (40 bytes)
        int32_t width;              // Width of the image
        int32_t height;             // Height of the image
        uint16_t planes;            // Number of color planes (must be 1)
        uint16_t bitCount;          // Bits per pixel (1, 4, 8, 24, or 32)
        uint32_t compression;       // Compression method (0 = uncompressed)
        uint32_t imageSize;         // Size of raw bitmap data
        int32_t xPixelsPerMeter;    // Horizontal resolution
        int32_t yPixelsPerMeter;    // Vertical resolution
        uint32_t colorsUsed;        // Number of colors in the palette
        uint32_t colorsImportant;   // Important colors
    };

    BMPFileHeader fileHeader;
    BMPInfoHeader infoHeader;

    // Read file header
    file.read(reinterpret_cast<char*>(&fileHeader.fileType), sizeof(fileHeader.fileType));
    file.read(reinterpret_cast<char*>(&fileHeader.fileSize), sizeof(fileHeader.fileSize));
    file.read(reinterpret_cast<char*>(&fileHeader.reserved1), sizeof(fileHeader.reserved1));
    file.read(reinterpret_cast<char*>(&fileHeader.reserved2), sizeof(fileHeader.reserved2));
    file.read(reinterpret_cast<char*>(&fileHeader.offsetData), sizeof(fileHeader.offsetData));

    // Validate BMP signature
    if (fileHeader.fileType != 0x4D42) {
        file.close();
        RAISE_RUNTIME_ERROR << "ImageTools::readBitmap: Invalid BMP file signature in " << path;
    }

    // Read info header
    file.read(reinterpret_cast<char*>(&infoHeader.headerSize), sizeof(infoHeader.headerSize));
    file.read(reinterpret_cast<char*>(&infoHeader.width), sizeof(infoHeader.width));
    file.read(reinterpret_cast<char*>(&infoHeader.height), sizeof(infoHeader.height));
    file.read(reinterpret_cast<char*>(&infoHeader.planes), sizeof(infoHeader.planes));
    file.read(reinterpret_cast<char*>(&infoHeader.bitCount), sizeof(infoHeader.bitCount));
    file.read(reinterpret_cast<char*>(&infoHeader.compression), sizeof(infoHeader.compression));
    file.read(reinterpret_cast<char*>(&infoHeader.imageSize), sizeof(infoHeader.imageSize));
    file.read(reinterpret_cast<char*>(&infoHeader.xPixelsPerMeter), sizeof(infoHeader.xPixelsPerMeter));
    file.read(reinterpret_cast<char*>(&infoHeader.yPixelsPerMeter), sizeof(infoHeader.yPixelsPerMeter));
    file.read(reinterpret_cast<char*>(&infoHeader.colorsUsed), sizeof(infoHeader.colorsUsed));
    file.read(reinterpret_cast<char*>(&infoHeader.colorsImportant), sizeof(infoHeader.colorsImportant));

    // Validate header
    if (infoHeader.compression != 0) {
        file.close();
        RAISE_RUNTIME_ERROR << "ImageTools::readBitmap: Compressed BMP files are not supported";
    }

    // Determine image properties
    const int width = infoHeader.width;
    const int height = std::abs(infoHeader.height);
    const bool topDown = infoHeader.height < 0;
    const int bitCount = infoHeader.bitCount;

    // Calculate row size (rows are padded to 4-byte boundaries)
    const int bytesPerPixel = bitCount / 8;
    const int rowSize = ((width * bitCount + 31) / 32) * 4;

    // Determine number of channels and OpenCV type
    int channels = 0;
    int cvType = CV_8U;

    if (bitCount == 8) {
        channels = 1; // Grayscale
    } else if (bitCount == 24) {
        channels = 3; // BGR
    } else if (bitCount == 32) {
        channels = 4; // BGRA
    } else {
        file.close();
        RAISE_RUNTIME_ERROR << "ImageTools::readBitmap: Unsupported bit depth: " << bitCount;
    }

    // Read color palette for 8-bit images
    std::vector<uint8_t> palette;
    if (bitCount == 8) {
        const uint32_t paletteColors = infoHeader.colorsUsed > 0 ? infoHeader.colorsUsed : 256;
        palette.resize(paletteColors * 4); // Each palette entry is 4 bytes (BGRA)
        // Seek to palette position (right after info header)
        file.seekg(14 + infoHeader.headerSize, std::ios::beg);
        file.read(reinterpret_cast<char*>(palette.data()), palette.size());
    }

    // Create output Mat
    output.create(height, width, CV_MAKETYPE(cvType, channels));
    cv::Mat mat = output.getMat();

    // Seek to pixel data
    file.seekg(fileHeader.offsetData, std::ios::beg);

    // Read pixel data
    std::vector<uint8_t> rowBuffer(rowSize);

    for (int y = 0; y < height; ++y) {
        // Calculate the actual row index (BMP stores bottom-to-top by default)
        int dstRow = topDown ? y : (height - 1 - y);

        // Read row data
        file.read(reinterpret_cast<char*>(rowBuffer.data()), rowSize);
        if (!file) {
            file.close();
            RAISE_RUNTIME_ERROR << "ImageTools::readBitmap: Error reading pixel data at row " << y;
        }

        uint8_t* dstPtr = mat.ptr<uint8_t>(dstRow);

        if (bitCount == 8) {
            // 8-bit indexed color - convert using palette
            for (int x = 0; x < width; ++x) {
                const uint8_t paletteIndex = rowBuffer[x];
                if (paletteIndex * 4 < palette.size()) {
                    // Use the blue component as grayscale value (or calculate luminance)
                    const uint8_t b = palette[paletteIndex * 4];
                    const uint8_t g = palette[paletteIndex * 4 + 1];
                    const uint8_t r = palette[paletteIndex * 4 + 2];
                    // Convert to grayscale using luminance formula
                    dstPtr[x] = static_cast<uint8_t>(0.299 * r + 0.587 * g + 0.114 * b);
                } else {
                    dstPtr[x] = 0;
                }
            }
        } else if (bitCount == 24) {
            // 24-bit BGR
            for (int x = 0; x < width; ++x) {
                const int srcIdx = x * 3;
                const int dstIdx = x * 3;
                dstPtr[dstIdx + 0] = rowBuffer[srcIdx + 0]; // B
                dstPtr[dstIdx + 1] = rowBuffer[srcIdx + 1]; // G
                dstPtr[dstIdx + 2] = rowBuffer[srcIdx + 2]; // R
            }
        } else if (bitCount == 32) {
            // 32-bit BGRA
            for (int x = 0; x < width; ++x) {
                const int srcIdx = x * 4;
                const int dstIdx = x * 4;
                dstPtr[dstIdx + 0] = rowBuffer[srcIdx + 0]; // B
                dstPtr[dstIdx + 1] = rowBuffer[srcIdx + 1]; // G
                dstPtr[dstIdx + 2] = rowBuffer[srcIdx + 2]; // R
                dstPtr[dstIdx + 3] = rowBuffer[srcIdx + 3]; // A
            }
        }
    }

    file.close();

    const DataType dt = CVTools::getMatDataType(mat);

    // convert BGR(A) to RGB(A) if needed
    if (mat.channels() == 3 && dt == DataType::DT_Byte) {
        cv::cvtColor(mat, mat, cv::COLOR_BGR2RGB);
    }
    else if (mat.channels() == 4 && dt == DataType::DT_Byte) {
        cv::cvtColor(mat, mat, cv::COLOR_BGRA2RGBA);
    }

    SLIDEIO_LOG(INFO) << "ImageTools::readBitmap: Successfully read BMP file " << path 
        << " (" << width << "x" << height << ", " << bitCount << " bits)";
}
