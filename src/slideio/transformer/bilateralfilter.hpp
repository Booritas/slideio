// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "transformation.hpp"

namespace slideio
{

    class SLIDEIO_TRANSFORMER_EXPORTS BilateralFilter : public Transformation
    {
    public:
        BilateralFilter()
        {
            m_type = TransformationType::BilateralFilter;
        }

        int getDiameter() const
        {
            return m_diameter;
        }

        void setDiameter(int diameter)
        {
            m_diameter = diameter;
        }

        double getSigmaColor() const
        {
            return m_sigmaColor;
        }

        void setSigmaColor(double sigmaColor)
        {
            m_sigmaColor = sigmaColor;
        }

        double getSigmaSpace() const
        {
            return m_sigmaSpace;
        }

        void setSigmaSpace(double sigmaSpace)
        {
            m_sigmaSpace = sigmaSpace;
        }

        void applyTransformation(const cv::Mat& block, cv::OutputArray transformedBlock) const override;
        std::vector<DataType> computeChannelDataTypes(const std::vector<DataType>& channels) const override;
        int getInflationValue() const override;

    private:
        int m_diameter = 5;
        double m_sigmaColor = 1.;
        double m_sigmaSpace = 1.;
    };
}
