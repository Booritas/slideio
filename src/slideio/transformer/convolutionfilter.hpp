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
        GaussianBlurFilter() {
            m_type = TransformationType::GaussianBlurFilter;
        }
    private:
        int m_kernelSizeX = 5;
        int m_kernelSizeY = 5;
        double m_sigmaX = 0;
        double m_sigmaY = 0;

    public:
        int getKernelSizeX() const {
            return m_kernelSizeX;
        }

        void setKernelSizeX(int kernelSizeX) {
            m_kernelSizeX = kernelSizeX;
        }

        int getKernelSizeY() const {
            return m_kernelSizeY;
        }

        void setKernelSizeY(int kernelSizeY) {
            m_kernelSizeY = kernelSizeY;
        }

        double getSigmaX() const {
            return m_sigmaX;
        }

        void setSigmaX(double sigmaX) {
            m_sigmaX = sigmaX;
        }

        double getSigmaY() const {
            return m_sigmaY;
        }

        void setSigmaY(double sigmaY) {
            m_sigmaY = sigmaY;
        }
    };

    class SLIDEIO_TRANSFORMER_EXPORTS MedianBlurFilter : public Transformation
    {
    public:
        MedianBlurFilter() {
            m_type = TransformationType::MedianBlurFilter;
        }
        virtual ~MedianBlurFilter() = default;
        int getKernelSize() const{
            return m_kernelSize;
        }
        void setKernelSize(int kernelSize) {
            m_kernelSize = kernelSize;
        }
    private:
        int m_kernelSize = 5;

    };

    class SLIDEIO_TRANSFORMER_EXPORTS SobelFilter : public Transformation
    {
    public:
        SobelFilter() {
            m_type = TransformationType::SobelFilter;
        }
        DataType getDepth() const {
            return m_depth;
        }
        void setDepth(const DataType& depth) {
            m_depth = depth;
        }
        int getDx() const {
            return m_dx;
        }
        void setDx(int dx) {
            m_dx = dx;
        }
        int getDy() const {
            return m_dy;
        }
        void setDy(int dy) {
            m_dy = dy;
        }
        int getKernelSize() const {
            return m_ksize;
        }
        void setKernelSize(int ksize) {
            m_ksize = ksize;
        }
        double getScale() const {
            return m_scale;
        }
        void setScale(double scale) {
            m_scale = scale;
        }
        double getDelta() const {
            return m_delta;
        }
        void setDelta(double delta) {
            m_delta = delta;
        }
    private:
        DataType m_depth = DataType::DT_Unknown;
        int m_dx = 1;
        int m_dy = 1;
        int m_ksize = 3;
        double m_scale = 1.;
        double m_delta = 0.;
    };

    class SLIDEIO_TRANSFORMER_EXPORTS ScharrFilter : public Transformation
    {
    public:
        ScharrFilter() {
            m_type = TransformationType::ScharrFilter;
        }
        DataType getDepth() const {
            return m_depth;
        }
        void setDepth(const DataType& depth) {
            m_depth = depth;
        }
        int getDx() const {
            return m_dx;
        }
        void setDx(int dx) {
            m_dx = dx;
        }
        int getDy() const {
            return m_dy;
        }
        void setDy(int dy) {
            m_dy = dy;
        }
        double getScale() const {
            return m_scale;
        }
        void setScale(double scale) {
            m_scale = scale;
        }
        double getDelta() const {
            return m_delta;
        }
        void setDelta(double delta) {
            m_delta = delta;
        }
    private:
        DataType m_depth = DataType::DT_Unknown;
        int m_dx = 1;
        int m_dy = 1;
        double m_scale = 1.;
        double m_delta = 0.;
    };
}
