// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/transformer/transformer_def.hpp"
#include "slideio/transformer/transformationex.hpp"
#include "slideio/transformer/transformationtype.hpp"

namespace slideio
{
    class SLIDEIO_TRANSFORMER_EXPORTS GaussianBlurFilter : public TransformationEx
    {
    public:
        GaussianBlurFilter()
        {
            m_type = TransformationType::GaussianBlurFilter;
        }

        GaussianBlurFilter(const GaussianBlurFilter& other)
            : TransformationEx(other),
              m_kernelSizeX(other.m_kernelSizeX),
              m_kernelSizeY(other.m_kernelSizeY),
              m_sigmaX(other.m_sigmaX),
              m_sigmaY(other.m_sigmaY) {
        }

        GaussianBlurFilter(GaussianBlurFilter&& other) noexcept
            : TransformationEx(std::move(other)),
              m_kernelSizeX(other.m_kernelSizeX),
              m_kernelSizeY(other.m_kernelSizeY),
              m_sigmaX(other.m_sigmaX),
              m_sigmaY(other.m_sigmaY) {
        }

        GaussianBlurFilter& operator=(const GaussianBlurFilter& other) {
            if (this == &other)
                return *this;
            TransformationEx::operator =(other);
            m_kernelSizeX = other.m_kernelSizeX;
            m_kernelSizeY = other.m_kernelSizeY;
            m_sigmaX = other.m_sigmaX;
            m_sigmaY = other.m_sigmaY;
            return *this;
        }

        GaussianBlurFilter& operator=(GaussianBlurFilter&& other) noexcept {
            if (this == &other)
                return *this;
            TransformationEx::operator =(std::move(other));
            m_kernelSizeX = other.m_kernelSizeX;
            m_kernelSizeY = other.m_kernelSizeY;
            m_sigmaX = other.m_sigmaX;
            m_sigmaY = other.m_sigmaY;
            return *this;
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
