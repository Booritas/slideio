// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_vsiimagedriver_HPP
#define OPENCV_slideio_vsiimagedriver_HPP

#include "slideio/drivers/vsi/vsi_api_def.hpp"
#include "slideio/core/imagedriver.hpp"

namespace slideio
{
    class SLIDEIO_VSI_EXPORTS VSIImageDriver : public slideio::ImageDriver
    {
    public:
        VSIImageDriver();
        ~VSIImageDriver();
        std::string getID() const override;
        std::shared_ptr<slideio::CVSlide> openFile(const std::string& filePath) override;
        std::string getFileSpecs() const override;
    };
}
#endif
