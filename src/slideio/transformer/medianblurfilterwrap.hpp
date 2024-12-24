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
    class MedianBlurFilter;
    enum class TransformationType;
    class SLIDEIO_TRANSFORMER_EXPORTS MedianBlurFilterWrap : public TransformationWrapper
    {
    public:
        MedianBlurFilterWrap(const MedianBlurFilterWrap& other)
            : TransformationWrapper(other),
              m_filter(other.m_filter) {
        }

        MedianBlurFilterWrap(MedianBlurFilterWrap&& other) noexcept
            : TransformationWrapper(std::move(other)),
              m_filter(std::move(other.m_filter)) {
        }

        MedianBlurFilterWrap& operator=(const MedianBlurFilterWrap& other) {
            if (this == &other)
                return *this;
            TransformationWrapper::operator =(other);
            m_filter = other.m_filter;
            return *this;
        }

        MedianBlurFilterWrap& operator=(MedianBlurFilterWrap&& other) noexcept {
            if (this == &other)
                return *this;
            TransformationWrapper::operator =(std::move(other));
            m_filter = std::move(other.m_filter);
            return *this;
        }

        MedianBlurFilterWrap();
        MedianBlurFilterWrap(const MedianBlurFilter& filter);
        int getKernelSize() const;
        void setKernelSize(int kernelSize);
        TransformationType getType() const override;
        std::shared_ptr<MedianBlurFilter> getFilter() const;

    private:
        std::shared_ptr<MedianBlurFilter> m_filter;
};

}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
