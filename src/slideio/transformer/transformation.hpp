// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "transformer_def.hpp"

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

    };

    class Transformation
    {
    public:
        Transformation() {
            m_type = TransformationType::Unknown;
        }
        TransformationType getType() const {
            return m_type;
        }
    protected:
        TransformationType m_type;
    };
}
