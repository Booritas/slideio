// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_czismallimage_HPP
#define OPENCV_slideio_czismallimage_HPP

#include "slideio/drivers/czi/cziauximage.hpp"
#include "slideio/slideio_def.hpp"
#include "slideio/core/cvsmallscene.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SLIDEIO_EXPORTS CZISmallImage : public CZIAuxImage
    {
    public:
        bool init() override;
    protected:
        void readImage(cv::OutputArray output) override;
    };
};

#endif
