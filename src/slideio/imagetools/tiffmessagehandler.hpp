// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/imagetools/slideio_imagetools_def.hpp"

namespace slideio {

    /// Installs the slideio error and warning handlers into libtiff. Idempotent
    /// and thread-safe; call from library initialisation. The handlers are
    /// process-global, so they are installed once and never swapped again --
    /// swapping them per object was a data race as soon as two threads could
    /// read at the same time.
    SLIDEIO_IMAGETOOLS_EXPORTS void installTiffMessageHandlers();
}
