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
    class ScharrFilter;
    enum class DataType;
    enum class TransformationType;

    class SLIDEIO_TRANSFORMER_EXPORTS ScharrFilterWrap : public TransformationWrapper
    {
    public:
        ScharrFilterWrap(const ScharrFilterWrap& other)
            : TransformationWrapper(other),
              m_filter(other.m_filter) {
        }

        ScharrFilterWrap(ScharrFilterWrap&& other) noexcept
            : TransformationWrapper(std::move(other)),
              m_filter(std::move(other.m_filter)) {
        }

        ScharrFilterWrap& operator=(const ScharrFilterWrap& other) {
            if (this == &other)
                return *this;
            TransformationWrapper::operator =(other);
            m_filter = other.m_filter;
            return *this;
        }

        ScharrFilterWrap& operator=(ScharrFilterWrap&& other) noexcept {
            if (this == &other)
                return *this;
            TransformationWrapper::operator =(std::move(other));
            m_filter = std::move(other.m_filter);
            return *this;
        }

        ScharrFilterWrap();
        ScharrFilterWrap(const ScharrFilter& filter);
        DataType getDepth() const;
        void setDepth(const DataType& depth);
        int getDx() const;
        void setDx(int dx);
        int getDy() const;
        void setDy(int dy);
        double getScale() const;
        void setScale(double scale);
        double getDelta() const;
        void setDelta(double delta);
        TransformationType getType() const  override;
        std::shared_ptr<ScharrFilter> getFilter() const;

    private:
        std::shared_ptr<ScharrFilter> m_filter;
    };

}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
