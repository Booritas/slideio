// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/transformer/transformationex.hpp"
#include "slideio/transformer/transformationtype.hpp"
#include "slideio/base/slideio_enums.hpp"

namespace slideio
{
    class SLIDEIO_TRANSFORMER_EXPORTS ScharrFilter : public TransformationEx
    {
    public:
        ScharrFilter() {
            m_type = TransformationType::ScharrFilter;
        }

        ScharrFilter(const ScharrFilter& other)
            : TransformationEx(other),
              m_depth(other.m_depth),
              m_dx(other.m_dx),
              m_dy(other.m_dy),
              m_scale(other.m_scale),
              m_delta(other.m_delta) {
        }

        ScharrFilter(ScharrFilter&& other) noexcept
            : TransformationEx(std::move(other)),
              m_depth(other.m_depth),
              m_dx(other.m_dx),
              m_dy(other.m_dy),
              m_scale(other.m_scale),
              m_delta(other.m_delta) {
        }

        ScharrFilter& operator=(const ScharrFilter& other) {
            if (this == &other)
                return *this;
            TransformationEx::operator =(other);
            m_depth = other.m_depth;
            m_dx = other.m_dx;
            m_dy = other.m_dy;
            m_scale = other.m_scale;
            m_delta = other.m_delta;
            return *this;
        }

        ScharrFilter& operator=(ScharrFilter&& other) noexcept {
            if (this == &other)
                return *this;
            TransformationEx::operator =(std::move(other));
            m_depth = other.m_depth;
            m_dx = other.m_dx;
            m_dy = other.m_dy;
            m_scale = other.m_scale;
            m_delta = other.m_delta;
            return *this;
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

        void applyTransformation(const cv::Mat& block, cv::OutputArray transformedBlock) const override;
        int getInflationValue() const override;
        std::vector<DataType> computeChannelDataTypes(const std::vector<DataType>& channels) const override;

    private:
        DataType m_depth = DataType::DT_Float32;
        int m_dx = 1;
        int m_dy = 1;
        double m_scale = 1.;
        double m_delta = 0.;
    };
}
