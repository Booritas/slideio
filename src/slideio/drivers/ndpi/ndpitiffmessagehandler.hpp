// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/drivers/ndpi/ndpi_api_def.hpp"

namespace slideio {

    class SLIDEIO_NDPI_EXPORTS NDPITIFFMessageHandler
    {
    public:
        NDPITIFFMessageHandler();
        ~NDPITIFFMessageHandler();
        // Copying would save the same two handlers twice and restore them twice, the
        // second time over whatever the intervening scope installed. Copy was never
        // meaningful; NDPITIFFKeeper deletes its copy for the analogous reason.
        NDPITIFFMessageHandler(const NDPITIFFMessageHandler&) = delete;
        NDPITIFFMessageHandler& operator=(const NDPITIFFMessageHandler&) = delete;
    private:
        void* m_oldWarningHandler;
        void* m_oldErrorHandler;
    };
}
