// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <stdlib.h>
#include "slideio/drivers/ndpi/ndpitiffmessagehandler.hpp"
#include "slideio/core/tools/log.hpp"
#include "slideio/core/tools/exceptions.hpp"

using namespace slideio;

void NDPITIFFWarningHandler(const char *module, const char *fmt, va_list ap) {
    if(fmt && *fmt) {
        char* msg(nullptr);
        vasprintf(&msg, fmt, ap);
        if(msg) {
            SLIDEIO_LOG(WARNING) << "TIFF Warning:" << msg;
            free(msg);
        }
    }
}

void NDPITIFFErrorHandler(const char *module, const char *fmt, va_list ap) {
    if(fmt && *fmt) {
        char* msg(nullptr);
        vasprintf(&msg, fmt, ap);
        if(msg) {
            std::string message(msg);
            free(msg);
            RAISE_RUNTIME_ERROR << "TIFF Error:" << message;
        }
    }
}

NDPITIFFMessageHandler::NDPITIFFMessageHandler() {
    m_oldErrorHandler = (void*)TIFFSetErrorHandler(NDPITIFFErrorHandler);
    m_oldWarningHandler = (void*)TIFFSetWarningHandler(NDPITIFFWarningHandler);
}

NDPITIFFMessageHandler::~NDPITIFFMessageHandler() {
    TIFFSetErrorHandler((TIFFErrorHandler)m_oldErrorHandler);
    TIFFSetWarningHandler((TIFFErrorHandler)m_oldWarningHandler);
}
