// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_gdalimagedriver_HPP
#define OPENCV_slideio_gdalimagedriver_HPP

#include "slideio/drivers/gdal/gdal_api_def.hpp"
#include "slideio/core/imagedriver.hpp"
#include "slideio/core/cvslide.hpp"
#include <opencv2/core.hpp>

namespace slideio
{
    class SLIDEIO_GDAL_EXPORTS GDALImageDriver : public slideio::ImageDriver
    {
    public:
        GDALImageDriver();
        ~GDALImageDriver();
        std::string getID() const override;
        std::shared_ptr<CVSlide> openFile(const std::string& filePath) override;
        std::string getFileSpecs() const override;
    };
}

#endif
