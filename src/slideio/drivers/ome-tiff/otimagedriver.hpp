// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/ome-tiff/ot_api_def.hpp"
#include "slideio/core/imagedriver.hpp"

namespace slideio
{
    namespace ometiff
    {
        class SLIDEIO_OMETIFF_EXPORTS OTImageDriver : public slideio::ImageDriver
        {
        public:
            OTImageDriver();
            ~OTImageDriver();
            std::string getID() const override;
            std::shared_ptr<slideio::CVSlide> openFile(const std::string& filePath) override;
            std::string getFileSpecs() const override;
        };
    }
}

