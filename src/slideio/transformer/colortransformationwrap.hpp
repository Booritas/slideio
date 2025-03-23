// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/transformer/transformer_def.hpp"
#include "slideio/transformer/transformationwrapper.hpp"
#include <memory>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4275 4251)
#endif


namespace slideio
{
    class ColorTransformation;
    enum class ColorSpace;
    enum class TransformationType;
    class SLIDEIO_TRANSFORMER_EXPORTS ColorTransformationWrap : public TransformationWrapper
    {
    public:
        ColorTransformationWrap(const ColorTransformationWrap& other)
            : TransformationWrapper(other),
              m_filter(other.m_filter) {
        }

        ColorTransformationWrap(ColorTransformationWrap&& other) noexcept
            : TransformationWrapper(std::move(other)),
              m_filter(std::move(other.m_filter)) {
        }

        ColorTransformationWrap& operator=(const ColorTransformationWrap& other) {
            if (this == &other)
                return *this;
            TransformationWrapper::operator =(other);
            m_filter = other.m_filter;
            return *this;
        }

        ColorTransformationWrap& operator=(ColorTransformationWrap&& other) noexcept {
            if (this == &other)
                return *this;
            TransformationWrapper::operator =(std::move(other));
            m_filter = std::move(other.m_filter);
            return *this;
        }

        ColorTransformationWrap();
        ColorTransformationWrap(const ColorTransformation& filter);
        ColorSpace getColorSpace() const;
        void setColorSpace(ColorSpace colorSpace);
        TransformationType getType() const override;
        std::shared_ptr<ColorTransformation> getFilter() const;;
    private:
        std::shared_ptr<ColorTransformation> m_filter;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
