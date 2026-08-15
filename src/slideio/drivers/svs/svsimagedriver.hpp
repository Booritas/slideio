// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/svs/svs_api_def.hpp"
#include "slideio/core/imagedriver.hpp"

namespace slideio
{
    // The aperio svs id. PHTIFF_DRIVER_ID stays declared here too: it remains the public
    // id string for the philips driver (see PHTIFFImageDriver), so openSlide(path,
    // "PHTIFF") keeps working, though it no longer steers control flow in this class.
    constexpr const char* SVS_DRIVER_ID = "SVS";
    constexpr const char* PHTIFF_DRIVER_ID = "PHTIFF";

    class SLIDEIO_SVS_EXPORTS SVSImageDriver : public slideio::ImageDriver
    {
    public:
        SVSImageDriver();
        ~SVSImageDriver();
        std::string getID() const override;
        std::shared_ptr<slideio::CVSlide> openFile(const std::string& filePath) override;
        std::string getFileSpecs() const override;
    };
}
