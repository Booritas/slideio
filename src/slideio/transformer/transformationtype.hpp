// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/transformer/transformer_def.hpp"
#include <ostream>

namespace slideio 
{
    enum class TransformationType
    {
        Unknown,
        ColorTransformation,
        GaussianBlurFilter,
        MedianBlurFilter,
        SobelFilter,
        ScharrFilter,
        LaplacianFilter,
        BilateralFilter,
        CannyFilter,
    };

    SLIDEIO_TRANSFORMER_EXPORTS std::ostream& operator << (std::ostream& os, TransformationType type);
}
