// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_ndpimessagehandler_HPP
#define OPENCV_slideio_ndpimessagehandler_HPP
#include <tiffio.h>

namespace slideio {

    class NDPITIFFMessageHandler
    {
    public:
        NDPITIFFMessageHandler();
        ~NDPITIFFMessageHandler();
    private:
        void* m_oldWarningHandler;
        void* m_oldErrorHandler;
    };
}

#endif