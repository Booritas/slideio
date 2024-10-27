// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/slideio_core_def.hpp"
#include <unordered_map>
#include <string>
#include <memory>
#include "slideio-opencv/core.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio {
    class ITileVisitor;
    class SLIDEIO_CORE_EXPORTS BlockTiler {
    public:
        BlockTiler(const cv::Mat& block, const cv::Size& tileSize);
        void apply(ITileVisitor* visitor) const;
        void apply(std::function<void(int, int, const cv::Mat&)> visitor) const;
    private:
        cv::Size m_tileSize;
        cv::Mat m_block;
    };
}