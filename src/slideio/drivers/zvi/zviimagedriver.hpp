// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_zviimagedriver_HPP
#define OPENCV_slideio_zviimagedriver_HPP

#include "slideio/drivers/zvi/zvi_api_def.hpp"
#include "slideio/core/imagedriver.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio-opencv/core.hpp"
#include <string>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SLIDEIO_ZVI_EXPORTS ZVIImageDriver : public slideio::ImageDriver
    {
    public:
        ZVIImageDriver();
        ~ZVIImageDriver();
        std::string getID() const override;
        std::shared_ptr<CVSlide> openFile(const std::string& filePath) override;
        std::string getFileSpecs() const override;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif
