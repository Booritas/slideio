// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/transformer/transformationex.hpp"
#include "slideio/base/slideio_enums.hpp"
#include "slideio/transformer/transformationtype.hpp"

namespace slideio
{

    class SLIDEIO_TRANSFORMER_EXPORTS SobelFilter : public TransformationEx
    {
    public:
        SobelFilter()
        {
            m_type = TransformationType::SobelFilter;
        }

        DataType getDepth() const
        {
            return m_depth;
        }

        void setDepth(const DataType& depth)
        {
            m_depth = depth;
        }

        int getDx() const
        {
            return m_dx;
        }

        void setDx(int dx)
        {
            m_dx = dx;
        }

        int getDy() const
        {
            return m_dy;
        }

        void setDy(int dy)
        {
            m_dy = dy;
        }

        int getKernelSize() const
        {
            return m_ksize;
        }

        void setKernelSize(int ksize)
        {
            m_ksize = ksize;
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
        int m_dx = 1;
        int m_dy = 1;
        int m_ksize = 3;
        double m_scale = 1.;
        double m_delta = 0.;
    };

}
