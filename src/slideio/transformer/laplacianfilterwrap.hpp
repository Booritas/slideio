// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/transformer/transformer_def.hpp"
#include <memory>

namespace slideio
{
    class LaplacianFilter;
    enum class DataType;
    enum class TransformationType;

    class SLIDEIO_TRANSFORMER_EXPORTS LaplacianFilterWrap
    {
    public:
        LaplacianFilterWrap();
        DataType getDepth() const;
        void setDepth(const DataType& depth);
        int getKernelSize() const;
        void setKernelSize(int kernelSize);
        double getScale() const;
        void setScale(double scale);
        double getDelta() const;
        void setDelta(double delta);
        TransformationType getType() const;
    private:
        std::shared_ptr<LaplacianFilter> m_filter;
    };
}
