// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/transformer/transformationex.hpp"
#include "slideio/transformer/transformationtype.hpp"

namespace slideio
{

    class SLIDEIO_TRANSFORMER_EXPORTS CannyFilter : public TransformationEx
    {
    public:
        CannyFilter(const CannyFilter& other)
            : TransformationEx(other),
              m_threshold1(other.m_threshold1),
              m_threshold2(other.m_threshold2),
              m_apertureSize(other.m_apertureSize),
              m_L2gradient(other.m_L2gradient) {
        }

        CannyFilter(CannyFilter&& other) noexcept
            : TransformationEx(std::move(other)),
              m_threshold1(other.m_threshold1),
              m_threshold2(other.m_threshold2),
              m_apertureSize(other.m_apertureSize),
              m_L2gradient(other.m_L2gradient) {
        }

        CannyFilter& operator=(const CannyFilter& other) {
            if (this == &other)
                return *this;
            TransformationEx::operator =(other);
            m_threshold1 = other.m_threshold1;
            m_threshold2 = other.m_threshold2;
            m_apertureSize = other.m_apertureSize;
            m_L2gradient = other.m_L2gradient;
            return *this;
        }

        CannyFilter& operator=(CannyFilter&& other) noexcept {
            if (this == &other)
                return *this;
            TransformationEx::operator =(std::move(other));
            m_threshold1 = other.m_threshold1;
            m_threshold2 = other.m_threshold2;
            m_apertureSize = other.m_apertureSize;
            m_L2gradient = other.m_L2gradient;
            return *this;
        }

        CannyFilter()
        {
            m_type = TransformationType::CannyFilter;
        }

        double getThreshold1() const
        {
            return m_threshold1;
        }

        void setThreshold1(double threshold1)
        {
            m_threshold1 = threshold1;
        }

        double getThreshold2() const
        {
            return m_threshold2;
        }

        void setThreshold2(double threshold2)
        {
            m_threshold2 = threshold2;
        }

        int getApertureSize() const
        {
            return m_apertureSize;
        }

        void setApertureSize(int apertureSize)
        {
            m_apertureSize = apertureSize;
        }

        bool getL2Gradient() const
        {
            return m_L2gradient;
        }

        void setL2Gradient(bool L2gradient)
        {
            m_L2gradient = L2gradient;
        }

        void applyTransformation(const cv::Mat& block, cv::OutputArray transformedBlock) const override;
        std::vector<DataType> computeChannelDataTypes(const std::vector<DataType>& channels) const override;
        int getInflationValue() const override;

    private:
        double m_threshold1 = 100.;
        double m_threshold2 = 200.;
        int m_apertureSize = 3;
        bool m_L2gradient = false;
    };
};
