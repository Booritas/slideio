// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/drivers/ndpi/ndpi_api_def.hpp"

namespace slideio
{
    /// Installs the slideio handlers into the NDPI libtiff fork. Idempotent and
    /// thread-safe. The fork has its own process-global handlers, separate from
    /// the regular libtiff's, so this is a second installation point rather
    /// than a duplicate of installTiffMessageHandlers().
    SLIDEIO_NDPI_EXPORTS void installNDPITiffMessageHandlers();
}
