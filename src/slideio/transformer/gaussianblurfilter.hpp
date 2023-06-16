// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "transformation.hpp"
#include "slideio/base/slideio_enums.hpp"

namespace slideio
{
    class SLIDEIO_TRANSFORMER_EXPORTS GaussianBlurFilter : public Transformation
    {
    public:
        GaussianBlurFilter()
        {
            m_type = TransformationType::GaussianBlurFilter;
        }
        int getKernelSizeX() const
        {
            return m_kernelSizeX;
        }

        void setKernelSizeX(int kernelSizeX)
        {
            m_kernelSizeX = kernelSizeX;
        }

        int getKernelSizeY() const
        {
            return m_kernelSizeY;
        }

        void setKernelSizeY(int kernelSizeY)
        {
            m_kernelSizeY = kernelSizeY;
        }

        double getSigmaX() const
        {
            return m_sigmaX;
        }

        void setSigmaX(double sigmaX)
        {
            m_sigmaX = sigmaX;
        }

        double getSigmaY() const
        {
            return m_sigmaY;
        }

        void setSigmaY(double sigmaY)
        {
            m_sigmaY = sigmaY;
        }

        void applyTransformation(const cv::Mat& block, cv::OutputArray transformedBlock) const override;
        int getInflationValue() const override;
    private:
        int m_kernelSizeX = 5;
        int m_kernelSizeY = 5;
        double m_sigmaX = 0;
        double m_sigmaY = 0;

    };
}
