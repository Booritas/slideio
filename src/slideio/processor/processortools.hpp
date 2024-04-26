// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#pragma once
#include "slideio/processor/slideio_processor_def.hpp"
#include <opencv2/core/types.hpp>

namespace slideio
{
    class SLIDEIO_PROCESSOR_EXPORTS ProcessorTools
    {
        public:
            static const cv::Point rotatePixelCW(const cv::Point& current, const cv::Point& center);
            static bool isBorderPoint(const cv::Point& point, const cv::Mat& tile, const cv::Point& tileOffset);
            static bool findFirstBorderPoint(const cv::Mat& tile, const cv::Point& tileOffset, cv::Point& borderPoint);
            static cv::Point rotatePointCW(const cv::Point& point, const cv::Point& center);
    };
}
