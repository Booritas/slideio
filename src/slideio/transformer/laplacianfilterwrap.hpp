// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/transformer/transformer_def.hpp"
#include "slideio/transformer/transformationwrapper.hpp"
#include "slideio/transformer/transformation.hpp"
#include <memory>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4275 4251)
#endif


namespace slideio
{
    class LaplacianFilter;
    enum class DataType;
    enum class TransformationType;

    class SLIDEIO_TRANSFORMER_EXPORTS LaplacianFilterWrap : public TransformationWrapper
    {
    public:
        LaplacianFilterWrap();

        LaplacianFilterWrap(const LaplacianFilterWrap& other)
            : TransformationWrapper(other),
              m_filter(other.m_filter) {
        }

        LaplacianFilterWrap(LaplacianFilterWrap&& other) noexcept
            : TransformationWrapper(std::move(other)),
              m_filter(std::move(other.m_filter)) {
        }

        LaplacianFilterWrap& operator=(const LaplacianFilterWrap& other) {
            if (this == &other)
                return *this;
            TransformationWrapper::operator =(other);
            m_filter = other.m_filter;
            return *this;
        }

        LaplacianFilterWrap& operator=(LaplacianFilterWrap&& other) noexcept {
            if (this == &other)
                return *this;
            TransformationWrapper::operator =(std::move(other));
            m_filter = std::move(other.m_filter);
            return *this;
        }

        LaplacianFilterWrap(const LaplacianFilter& filter);
        DataType getDepth() const;
        void setDepth(const DataType& depth);
        int getKernelSize() const;
        void setKernelSize(int kernelSize);
        double getScale() const;
        void setScale(double scale);
        double getDelta() const;
        void setDelta(double delta);
        TransformationType getType() const override;
        std::shared_ptr<Transformation> getFilter() const;;
    private:
        std::shared_ptr<LaplacianFilter> m_filter;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
