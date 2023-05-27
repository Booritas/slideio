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
        ColorTransformation
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

    enum class ColorSpace
    {
        RGB,
        GRAY,
        HSV,
        HLS,
        YUV,
        YCbCr,
        XYZ,
        LAB,
        LUV,
    };

    class ColorTransformation : public slideio::Transformation
    {
    public:
        ColorTransformation() {
            m_type = TransformationType::ColorTransformation;
            m_colorSpace = ColorSpace::RGB;
        }
        ColorSpace getColorSpace() const {
            return m_colorSpace;
        }
        void setColorSpace(ColorSpace colorSpace) {
            m_colorSpace = colorSpace;
        }
    private:
        ColorSpace m_colorSpace;
    };
}
