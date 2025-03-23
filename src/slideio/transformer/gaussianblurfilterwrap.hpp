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
    class GaussianBlurFilter;
    enum class TransformationType;
    class SLIDEIO_TRANSFORMER_EXPORTS GaussianBlurFilterWrap : public TransformationWrapper
    {
    public:
        GaussianBlurFilterWrap(const GaussianBlurFilterWrap& other)
            : TransformationWrapper(other),
              m_filter(other.m_filter) {
        }

        GaussianBlurFilterWrap(GaussianBlurFilterWrap&& other) noexcept
            : TransformationWrapper(std::move(other)),
              m_filter(std::move(other.m_filter)) {
        }

        GaussianBlurFilterWrap& operator=(const GaussianBlurFilterWrap& other) {
            if (this == &other)
                return *this;
            TransformationWrapper::operator =(other);
            m_filter = other.m_filter;
            return *this;
        }

        GaussianBlurFilterWrap& operator=(GaussianBlurFilterWrap&& other) noexcept {
            if (this == &other)
                return *this;
            TransformationWrapper::operator =(std::move(other));
            m_filter = std::move(other.m_filter);
            return *this;
        }

        GaussianBlurFilterWrap();
        GaussianBlurFilterWrap(const GaussianBlurFilter& filter);
        int getKernelSizeX() const;
        void setKernelSizeX(int kernelSizeX);
        int getKernelSizeY() const;
        void setKernelSizeY(int kernelSizeY);
        double getSigmaX() const;
        void setSigmaX(double sigmaX);
        double getSigmaY() const;
        void setSigmaY(double sigmaY);
        TransformationType getType() const override;
        std::shared_ptr<GaussianBlurFilter> getFilter() const;

    private:
        std::shared_ptr<GaussianBlurFilter> m_filter;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
