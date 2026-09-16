// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include <opencv2/core/types.hpp>

namespace slideio
{
    inline Rect::Rect(const cv::Rect& cvRect)
        : x(cvRect.x), y(cvRect.y), width(cvRect.width), height(cvRect.height)
    {
    }

    inline Rect& Rect::operator=(const cv::Rect& cvRect)
    {
        x = cvRect.x;
        y = cvRect.y;
        width = cvRect.width;
        height = cvRect.height;
        return *this;
    }

    inline Rect::operator cv::Rect() const
    {
        return cv::Rect(x, y, width, height);
    }
}

