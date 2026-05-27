// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/imagetools/slideio_imagetools_def.hpp"
#include "slideio/base/randomaccessstream.hpp"
#include "slideio/imagetools/libtiff.hpp"

#include <memory>

namespace slideio
{
    // Wraps any RandomAccessStream in libtiff's TIFFClientOpen API.
    // Returns a TIFF* that behaves identically to TIFFOpen(path)
    // for all read-only operations. The returned handle owns a
    // shared_ptr<RandomAccessStream>; closing the handle releases it.
    SLIDEIO_IMAGETOOLS_EXPORTS
    libtiff::TIFF* openTiffFromStream(std::shared_ptr<RandomAccessStream> stream);
}
