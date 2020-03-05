// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_imagetools_HPP
#define OPENCV_slideio_imagetools_HPP

#include "slideio/slideio_def.hpp"
#include "slideio/slideio.hpp"
#include <opencv2/core.hpp>

#if defined(WIN32)
#pragma warning( push )
#pragma warning(disable:4005)
#pragma warning( pop )
#endif

namespace slideio
{
    class SLIDEIO_EXPORTS ImageTools
    {
    public:
        static int dataTypeSize(slideio::DataType dt);
        static void readGDALImage(const std::string& path, cv::OutputArray output);
        static void readJxrImage(const std::string& path, cv::OutputArray output);
        static void decodeJxrBlock(const uint8_t* data, size_t size, cv::OutputArray output);
        // jpeg 2000 related methods
        static void readJp2KFile(const std::string& path, cv::OutputArray output);
        static void decodeJp2KStream(const std::vector<uint8_t>& data, cv::OutputArray output,
            const std::vector<int>& channelIndices = std::vector<int>(),
            bool forceYUV = false);
        static void scaleRect(const cv::Rect& srcRect, const cv::Size& newSize, cv::Rect& trgRect);
        static void scaleRect(const cv::Rect& srcRect, double scaleX, double scaleY, cv::Rect& trgRect);
    };
}

#endif