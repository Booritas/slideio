// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "transformation.hpp"
#include "slideio/base/slideio_enums.hpp"

namespace slideio
{

    class SLIDEIO_TRANSFORMER_EXPORTS MedianBlurFilter : public Transformation
    {
    public:
        MedianBlurFilter()
        {
            m_type = TransformationType::MedianBlurFilter;
        }

        virtual ~MedianBlurFilter() = default;

        int getKernelSize() const
        {
            return m_kernelSize;
        }

        void setKernelSize(int kernelSize)
        {
            m_kernelSize = kernelSize;
        }

        void applyTransformation(const cv::Mat& block, cv::OutputArray transformedBlock) const override;
        int getInflationValue() const override;

    private:
        int m_kernelSize = 5;
    };

}
