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
    class SobelFilter;
    enum class DataType;
    enum class TransformationType;
    
    class SLIDEIO_TRANSFORMER_EXPORTS SobelFilterWrap : public TransformationWrapper
    {
    public:
        SobelFilterWrap();

        SobelFilterWrap(const SobelFilterWrap& other)
            : TransformationWrapper(other),
              m_filter(other.m_filter) {
        }

        SobelFilterWrap(SobelFilterWrap&& other) noexcept
            : TransformationWrapper(std::move(other)),
              m_filter(std::move(other.m_filter)) {
        }

        SobelFilterWrap& operator=(const SobelFilterWrap& other) {
            if (this == &other)
                return *this;
            TransformationWrapper::operator =(other);
            m_filter = other.m_filter;
            return *this;
        }

        SobelFilterWrap& operator=(SobelFilterWrap&& other) noexcept {
            if (this == &other)
                return *this;
            TransformationWrapper::operator =(std::move(other));
            m_filter = std::move(other.m_filter);
            return *this;
        }

        SobelFilterWrap(const SobelFilter& filter);
        DataType getDepth() const;
        void setDepth(const DataType& depth);
        int getDx() const;
        void setDx(int dx);
        int getDy() const;
        void setDy(int dy);
        int getKernelSize() const;
        void setKernelSize(int ksize);
        double getScale() const;
        void setScale(double scale);
        double getDelta() const;
        void setDelta(double delta);
        TransformationType getType() const override;
        std::shared_ptr<SobelFilter> getFilter() const;

    private:
        std::shared_ptr<SobelFilter> m_filter;
    };

}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
