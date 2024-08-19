// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/transformer/transformer_def.hpp"
#include "slideio/transformer/transformationwrapper.hpp"
#include <memory>

namespace slideio
{
    class SobelFilter;
    enum class DataType;
    enum class TransformationType;
    
    class SLIDEIO_TRANSFORMER_EXPORTS SobelFilterWrap : public TransformationWrapper
    {
    public:
        SobelFilterWrap();
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
