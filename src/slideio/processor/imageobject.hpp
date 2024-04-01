// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#pragma once
#include <opencv2/core/types.hpp>

namespace slideio
{
    class ImageObject
    {
    public:
        ImageObject() : m_id(0), m_contourBegin(-1), m_pixelCount(0) {}
        int m_id;
        cv::Rect m_boundingRect;
        cv::Point m_innerPoint;
        int m_contourBegin;
        int m_pixelCount;
    };
}
