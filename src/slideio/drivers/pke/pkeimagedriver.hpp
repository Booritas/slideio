// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/PKE/PKE_api_def.hpp"
#include "slideio/core/imagedriver.hpp"

namespace slideio
{
    class SLIDEIO_PKE_EXPORTS PKEImageDriver : public slideio::ImageDriver
    {
    public:
        PKEImageDriver();
        ~PKEImageDriver();
        std::string getID() const override;
        std::shared_ptr<slideio::CVSlide> openFile(const std::string& filePath) override;
        std::string getFileSpecs() const override;
    };
}

