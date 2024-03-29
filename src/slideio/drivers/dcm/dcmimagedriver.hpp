﻿// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_dcmimagedriver_HPP
#define OPENCV_slideio_dcmimagedriver_HPP

#include "slideio/drivers/dcm/dcm_api_def.hpp"
#include "slideio/core/imagedriver.hpp"
#include "slideio/core/cvslide.hpp"
#include <opencv2/core.hpp>
#include <string>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SLIDEIO_DCM_EXPORTS DCMImageDriver : public slideio::ImageDriver
    {
    public:
        DCMImageDriver();
        ~DCMImageDriver();
        std::string getID() const override;
        std::shared_ptr<CVSlide> openFile(const std::string& filePath) override;
        std::string getFileSpecs() const override;
        static void initializeDCMTK();
        static void clieanUpDCMTK();
        bool canOpenFile(const std::string& filePath) const override;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif
