// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "transformer_def.hpp"

namespace slideio
{
    enum class SLIDEIO_TRANSFORMER_EXPORTS TransformationType
    {
        Unknown,
        ColorTransformation,
        GaussianBlurFilter,
        MedianBlurFilter,
        SobelFilter,
        ScharrFilter,

    };

    class SLIDEIO_TRANSFORMER_EXPORTS Transformation
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
