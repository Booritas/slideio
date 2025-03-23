// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/base/slideio_enums.hpp"
#include "slideio/transformer/transformer_def.hpp"
#include "slideio/transformer/transformationtype.hpp"
#include "slideio/transformer/transformationwrapper.hpp"
#include <memory>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4275 4251)
#endif


namespace slideio
{

    class BilateralFilter;
    class SLIDEIO_TRANSFORMER_EXPORTS BilateralFilterWrap : public TransformationWrapper
    {
    public:
        BilateralFilterWrap(const BilateralFilterWrap& other)
            : TransformationWrapper(other),
              m_filter(other.m_filter) {
        }

        BilateralFilterWrap(BilateralFilterWrap&& other) noexcept
            : TransformationWrapper(std::move(other)),
              m_filter(std::move(other.m_filter)) {
        }

        BilateralFilterWrap& operator=(const BilateralFilterWrap& other) {
            if (this == &other)
                return *this;
            TransformationWrapper::operator =(other);
            m_filter = other.m_filter;
            return *this;
        }

        BilateralFilterWrap& operator=(BilateralFilterWrap&& other) noexcept {
            if (this == &other)
                return *this;
            TransformationWrapper::operator =(std::move(other));
            m_filter = std::move(other.m_filter);
            return *this;
        }

        BilateralFilterWrap();
        BilateralFilterWrap(const BilateralFilter& filter);
        int getDiameter() const;
        void setDiameter(int diameter);
        double getSigmaColor() const;
        void setSigmaColor(double sigmaColor);
        double getSigmaSpace() const;
        void setSigmaSpace(double sigmaSpace);
        TransformationType getType() const override;
        std::shared_ptr<BilateralFilter> getFilter() const;
    private:
        std::shared_ptr<BilateralFilter> m_filter;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
