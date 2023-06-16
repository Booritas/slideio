// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "transformer_def.hpp"
#include "transformation.hpp"

namespace slideio
{
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

    class SLIDEIO_TRANSFORMER_EXPORTS ColorTransformation : public slideio::Transformation
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

        void applyTransformation(const cv::Mat& block, cv::OutputArray transformedBlock) const override;
        std::vector<DataType> computeChannelDataTypes(const std::vector<DataType>& channels) const override;

    private:
        ColorSpace m_colorSpace;
    };
}
