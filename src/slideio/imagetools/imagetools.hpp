// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_imagetools_HPP
#define OPENCV_slideio_imagetools_HPP

#include <opencv2/core.hpp>
#include "slideio/imagetools/slideio_imagetools_def.hpp"
#include "slideio/base/slideio_enums.hpp"
#include "slideio/imagetools/encodeparameters.hpp"

#if defined(WIN32)
#pragma warning( push )
#pragma warning(disable:4005)
#pragma warning( pop )
#endif

namespace slideio
{
    class SLIDEIO_IMAGETOOLS_EXPORTS ImageTools
    {
    public:
        static void readGDALImage(const std::string& path, cv::OutputArray output);
        static void writeRGBImage(const std::string& path, Compression compression, cv::Mat raster);
        static void writeTiffImage(const std::string& path, cv::Mat raster);
        static void readJxrImage(const std::string& path, cv::OutputArray output);
        static void decodeJxrBlock(const uint8_t* data, size_t size, cv::OutputArray output);
        static void decodeJpegStream(const uint8_t* data, size_t size, cv::OutputArray output);
        static void encodeJpeg(const cv::Mat& raster, std::vector<uint8_t>& encodedStream, const JpegEncodeParameters& params);
        // jpeg 2000 related methods
        static void readJp2KFile(const std::string& path, cv::OutputArray output);
        static void decodeJp2KStream(const std::vector<uint8_t>& data, cv::OutputArray output,
            const std::vector<int>& channelIndices = std::vector<int>(),
            bool forceYUV = false);
        static int encodeJp2KStream(const cv::Mat& mat, uint8_t* buffer, int bufferSize,
            const JP2KEncodeParameters& parameters);
        static double computeSimilarity(const cv::Mat& left, const cv::Mat& right, bool ignoreTypes=false);
        static double compareHistograms(const cv::Mat& leftM, const cv::Mat& rightM, int bins);
        static int dataTypeSize(slideio::DataType dt);
        template <typename Type>
        static void convertTo32bitChannels(Type* data, int width, int height, int numChannels, int32_t** channels)
        {
            const int pixelSize = numChannels;
            const int stride = pixelSize * width;
            Type* line = data;
            int channelShift = 0;
            for (int y = 0; y < height; ++y) {
                Type* pixel = line;
                for (int x = 0; x < width; ++x) {
                    for (int channelIndex = 0; channelIndex < numChannels; ++channelIndex) {
                        int32_t* channel = channels[channelIndex];
                        channel[channelShift] = static_cast<int32_t>(pixel[channelIndex]);
                    }
                    pixel += pixelSize;
                    channelShift++;
                }
                line += stride;
            }
        }
    };
}

#endif