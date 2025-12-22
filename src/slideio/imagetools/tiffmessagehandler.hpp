// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/imagetools/slideio_imagetools_def.hpp"

namespace slideio {

    class SLIDEIO_IMAGETOOLS_EXPORTS TIFFMessageHandler
    {
    public:
        TIFFMessageHandler();
        ~TIFFMessageHandler();
    private:
        void* m_oldWarningHandler;
        void* m_oldErrorHandler;
    };
}
