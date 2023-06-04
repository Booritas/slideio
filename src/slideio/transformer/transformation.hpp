// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <vector>
#include <opencv2/core/mat.hpp>

#include "transformer_def.hpp"
#include "slideio/base/slideio_enums.hpp"


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
        LaplacianFilter,
        BilateralFilter,
        CannyFilter,

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
        virtual std::vector<DataType> computeChannelDataTypes(const std::vector<DataType>& channels) const;
        virtual int getInflationValue() const;
        virtual void applyTransformation(const cv::Mat& block, cv::OutputArray transformedBlock) const = 0;
    protected:
        TransformationType m_type;
    };
}
