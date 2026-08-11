// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/svs/svs_api_def.hpp"
#include "slideio/core/imagedriver.hpp"

namespace slideio
{
    // SVSImageDriver serves two formats that share the tiff reading code: the aperio svs
    // format and the philips tiff format. The id decides which one an instance reads.
    constexpr const char* SVS_DRIVER_ID = "SVS";
    constexpr const char* PHTIFF_DRIVER_ID = "PHTIFF";

    class SLIDEIO_SVS_EXPORTS SVSImageDriver : public slideio::ImageDriver
    {
    public:
        SVSImageDriver(const std::string& driverId = SVS_DRIVER_ID);
        ~SVSImageDriver();
        std::string getID() const override;
        std::shared_ptr<slideio::CVSlide> openFile(const std::string& filePath) override;
        std::string getFileSpecs() const override;
        bool canOpenFile(const std::string& filePath) const override;
    private:
        std::string m_driverId;
    };
}
