// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/exceptions.hpp"
#include "slideio/core/tools/readcontext.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include <string>

namespace slideio::bif
{
    /// One libtiff handle, borrowed from a scene's ContextPool by one reader at a time.
    /// TiffTools::setCurrentDirectory positions by offset, so a fresh handle costs a
    /// descriptor, not a re-scan of the file.
    class BIFReadContext : public ReadContext
    {
    public:
        explicit BIFReadContext(const std::string& filePath)
            : keeper(TiffTools::openTiffFile(filePath)) {
            if (!keeper.isValid()) {
                RAISE_RUNTIME_ERROR << "BIF: cannot open file " << filePath;
            }
        }
        TIFFKeeper keeper;
    };
}
