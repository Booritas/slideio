// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/transformer/transformationex.hpp"
#include "slideio/base/slideio_enums.hpp"
#include "slideio/transformer/transformationtype.hpp"

namespace slideio
{

    class SLIDEIO_TRANSFORMER_EXPORTS LaplacianFilter : public TransformationEx
    {
    public:
        LaplacianFilter(const LaplacianFilter& other)
            : TransformationEx(other),
              m_depth(other.m_depth),
              m_kernelSize(other.m_kernelSize),
              m_scale(other.m_scale),
              m_delta(other.m_delta) {
        }

        LaplacianFilter(LaplacianFilter&& other) noexcept
            : TransformationEx(std::move(other)),
              m_depth(other.m_depth),
              m_kernelSize(other.m_kernelSize),
              m_scale(other.m_scale),
              m_delta(other.m_delta) {
        }

        LaplacianFilter& operator=(const LaplacianFilter& other) {
            if (this == &other)
                return *this;
            TransformationEx::operator =(other);
            m_depth = other.m_depth;
            m_kernelSize = other.m_kernelSize;
            m_scale = other.m_scale;
            m_delta = other.m_delta;
            return *this;
        }

        LaplacianFilter& operator=(LaplacianFilter&& other) noexcept {
            if (this == &other)
                return *this;
            TransformationEx::operator =(std::move(other));
            m_depth = other.m_depth;
            m_kernelSize = other.m_kernelSize;
            m_scale = other.m_scale;
            m_delta = other.m_delta;
            return *this;
        }

        LaplacianFilter()
        {
            m_type = TransformationType::LaplacianFilter;
        }

        DataType getDepth() const
        {
            return m_depth;
        }

        void setDepth(const DataType& depth)
        {
            m_depth = depth;
        }

        int getKernelSize() const
        {
            return m_kernelSize;
        }

        void setKernelSize(int kernelSize)
        {
            m_kernelSize = kernelSize;
        }

        double getScale() const
        {
            return m_scale;
        }

        void setScale(double scale)
        {
            m_scale = scale;
        }

        double getDelta() const
        {
            return m_delta;
        }

        void setDelta(double delta)
        {
            m_delta = delta;
        }

        void applyTransformation(const cv::Mat& block, cv::OutputArray transformedBlock) const override;
        int getInflationValue() const override;
        std::vector<DataType> computeChannelDataTypes(const std::vector<DataType>& channels) const override;

    private:
        DataType m_depth = DataType::DT_Float32;
        int m_kernelSize = 1;
        double m_scale = 1.;
        double m_delta = 0.;
    };
}
