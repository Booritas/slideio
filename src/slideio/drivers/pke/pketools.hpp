// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/PKE/PKE_api_def.hpp"
#include <opencv2/core.hpp>
#include <string>

namespace slideio
{
    class SLIDEIO_PKE_EXPORTS PKETools
    {
    public:
        // Extracts magnification value from image information string
        static int extractMagnifiation(const std::string& description);
        // Extracts resolution value from image information string
        static double extractResolution(const std::string& description);
    };
}
