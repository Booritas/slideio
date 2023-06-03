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

    private:
        int m_kernelSizeX = 5;
        int m_kernelSizeY = 5;
        double m_sigmaX = 0;
        double m_sigmaY = 0;

    public:
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
    };

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

    private:
        int m_kernelSize = 5;
    };

    class SLIDEIO_TRANSFORMER_EXPORTS SobelFilter : public Transformation
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

    private:
        DataType m_depth = DataType::DT_Float32;
        int m_dx = 1;
        int m_dy = 1;
        int m_ksize = 3;
        double m_scale = 1.;
        double m_delta = 0.;
    };

    class SLIDEIO_TRANSFORMER_EXPORTS ScharrFilter : public Transformation
    {
    public:
        ScharrFilter()
        {
            m_type = TransformationType::ScharrFilter;
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

    private:
        DataType m_depth = DataType::DT_Float32;
        int m_dx = 1;
        int m_dy = 1;
        double m_scale = 1.;
        double m_delta = 0.;
    };

    class SLIDEIO_TRANSFORMER_EXPORTS LaplacianFilter : public Transformation
    {
    public:
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

    private:
        DataType m_depth = DataType::DT_Float32;
        int m_kernelSize = 1;
        double m_scale = 1.;
        double m_delta = 0.;
    };

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

    private:
        int m_diameter = 5;
        double m_sigmaColor = 1.;
        double m_sigmaSpace = 1.;
    };

    class SLIDEIO_TRANSFORMER_EXPORTS CannyFilter : public Transformation
    {
    public:
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

    private:
        double m_threshold1 = 100.;
        double m_threshold2 = 200.;
        int m_apertureSize = 3;
        bool m_L2gradient = false;
    };
};
