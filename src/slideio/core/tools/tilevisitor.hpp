// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio-opencv/core.hpp"

namespace slideio
{
    class SLIDEIO_CORE_EXPORTS ITileVisitor
    {
    public:
        virtual ~ITileVisitor() = default;
        virtual void visit(int x, int y, const cv::Mat& tile) = 0;
    };
}