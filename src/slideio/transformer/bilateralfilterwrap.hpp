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
        BilateralFilterWrap();
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
