// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/transformer/transformationex.hpp"
#include "slideio/transformer/transformationtype.hpp"

namespace slideio
{

    class SLIDEIO_TRANSFORMER_EXPORTS BilateralFilter : public TransformationEx
    {
    public:
        BilateralFilter(const BilateralFilter& other)
            : TransformationEx(other),
              m_diameter(other.m_diameter),
              m_sigmaColor(other.m_sigmaColor),
              m_sigmaSpace(other.m_sigmaSpace) {
        }

        BilateralFilter(BilateralFilter&& other) noexcept
            : TransformationEx(std::move(other)),
              m_diameter(other.m_diameter),
              m_sigmaColor(other.m_sigmaColor),
              m_sigmaSpace(other.m_sigmaSpace) {
        }

        BilateralFilter& operator=(const BilateralFilter& other) {
            if (this == &other)
                return *this;
            TransformationEx::operator =(other);
            m_diameter = other.m_diameter;
            m_sigmaColor = other.m_sigmaColor;
            m_sigmaSpace = other.m_sigmaSpace;
            return *this;
        }

        BilateralFilter& operator=(BilateralFilter&& other) noexcept {
            if (this == &other)
                return *this;
            TransformationEx::operator =(std::move(other));
            m_diameter = other.m_diameter;
            m_sigmaColor = other.m_sigmaColor;
            m_sigmaSpace = other.m_sigmaSpace;
            return *this;
        }

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
