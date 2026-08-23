// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include <opencv2/core/types.hpp>

namespace slideio
{
    inline Size::Size(const cv::Size& cvSize)
        : width(cvSize.width), height(cvSize.height)
    {
    }

    inline Size& Size::operator=(const cv::Size& cvSize)
    {
        width = cvSize.width;
        height = cvSize.height;
        return *this;
    }

    inline Size::operator cv::Size() const
    {
        return cv::Size(width, height);
    }
}