// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/transformer/transformer_def.hpp"
#include "slideio/transformer/transformationtype.hpp"
#include <memory>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4275 4251)
#endif

namespace slideio
{
    class CannyFilter;
    class SLIDEIO_TRANSFORMER_EXPORTS CannyFilterWrap
    {
    public:
        CannyFilterWrap();
        double getThreshold1() const;
        void setThreshold1(double threshold1);
        double getThreshold2() const;
        void setThreshold2(double threshold2);
        int getApertureSize() const;
        void setApertureSize(int apertureSize);
        bool getL2Gradient() const;
        void setL2Gradient(bool L2gradient);
        TransformationType getType() const;
    private:
        std::shared_ptr<CannyFilter> m_filter;
    };
};

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
