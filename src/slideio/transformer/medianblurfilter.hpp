// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/transformer/transformationex.hpp"
#include "slideio/transformer/transformationtype.hpp"

namespace slideio
{

    class SLIDEIO_TRANSFORMER_EXPORTS MedianBlurFilter : public TransformationEx
    {
    public:
        MedianBlurFilter()
        {
            m_type = TransformationType::MedianBlurFilter;
        }

        MedianBlurFilter(const MedianBlurFilter& other)
            : TransformationEx(other),
              m_kernelSize(other.m_kernelSize) {
        }

        MedianBlurFilter(MedianBlurFilter&& other) noexcept
            : TransformationEx(std::move(other)),
              m_kernelSize(other.m_kernelSize) {
        }

        MedianBlurFilter& operator=(const MedianBlurFilter& other) {
            if (this == &other)
                return *this;
            TransformationEx::operator =(other);
            m_kernelSize = other.m_kernelSize;
            return *this;
        }

        MedianBlurFilter& operator=(MedianBlurFilter&& other) noexcept {
            if (this == &other)
                return *this;
            TransformationEx::operator =(std::move(other));
            m_kernelSize = other.m_kernelSize;
            return *this;
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
