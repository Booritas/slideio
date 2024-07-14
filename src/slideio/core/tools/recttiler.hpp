// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/slideio_core_def.hpp"
#include <opencv2/core.hpp>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio {
    class SLIDEIO_CORE_EXPORTS RectTiler {
    public:
        RectTiler(const cv::Size& rect, const cv::Size& tileSize, const cv::Size& tileOverlap = cv::Size(0,0));
        void apply(std::function<void(const cv::Rect&)> visitor) const;
    private:
        cv::Size m_rectSize;
        cv::Size m_tileSize;
        cv::Size m_tileOverlap;
    };
}