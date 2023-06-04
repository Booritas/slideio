// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <opencv2/core/types.hpp>
#include "transformer_def.hpp"

namespace slideio
{
    class Transformation;

    class SLIDEIO_TRANSFORMER_EXPORTS TransformerTools
    {
    public:
        static void computeInflatedRectParams(const cv::Size& sceneSize, const cv::Rect& blockRect, int invlationValue, const cv::Size& blockSize,
            cv::Rect& inflatedBlockRect, cv::Size& inflatedSize, cv::Point& blockPositionInInflatedRect);
    };
}
