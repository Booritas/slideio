// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/transformer/transformer_def.hpp"
#include "slideio/transformer/transformationex.hpp"
#include "slideio/transformer/colorspace.hpp"
#include "slideio/transformer/transformationtype.hpp"

namespace slideio
{
    class SLIDEIO_TRANSFORMER_EXPORTS ColorTransformation : public slideio::TransformationEx
    {
    public:
        ColorTransformation() {
            m_type = TransformationType::ColorTransformation;
            m_colorSpace = ColorSpace::RGB;
        }

        ColorTransformation(const ColorTransformation& other)
            : slideio::TransformationEx(other),
              m_colorSpace(other.m_colorSpace) {
        }

        ColorTransformation(ColorTransformation&& other) noexcept
            : slideio::TransformationEx(std::move(other)),
              m_colorSpace(other.m_colorSpace) {
        }

        ColorTransformation& operator=(const ColorTransformation& other) {
            if (this == &other)
                return *this;
            slideio::TransformationEx::operator =(other);
            m_colorSpace = other.m_colorSpace;
            return *this;
        }

        ColorTransformation& operator=(ColorTransformation&& other) noexcept {
            if (this == &other)
                return *this;
            slideio::TransformationEx::operator =(std::move(other));
            m_colorSpace = other.m_colorSpace;
            return *this;
        }

        ColorTransformation(ColorSpace colorSpace) {
            m_type = TransformationType::ColorTransformation;
            m_colorSpace = colorSpace;
        }
        ColorSpace getColorSpace() const {
            return m_colorSpace;
        }
        void setColorSpace(ColorSpace colorSpace) {
            m_colorSpace = colorSpace;
        }

        void applyTransformation(const cv::Mat& block, cv::OutputArray transformedBlock) const override;
        std::vector<DataType> computeChannelDataTypes(const std::vector<DataType>& channels) const override;

    private:
        ColorSpace m_colorSpace;
    };
}
