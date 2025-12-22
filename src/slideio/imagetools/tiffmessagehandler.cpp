// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <stdlib.h>
#include "slideio/imagetools/tiffmessagehandler.hpp"

#include <cstdarg>
#include <tiffio.h>

#include "slideio/base/log.hpp"
#include "slideio/base/exceptions.hpp"

using namespace slideio;

#ifdef _MSC_VER
int vasprintf(char** strp, const char* format, va_list ap)
{
    int len = _vscprintf(format, ap);
    if (len == -1)
        return -1;
    char* str = (char*)malloc((size_t)len + 1);
    if (!str)
        return -1;
    int retval = vsnprintf(str, len + 1, format, ap);
    if (retval == -1) {
        free(str);
        return -1;
    }
    *strp = str;
    return retval;
}

int asprintf(char** strp, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    int retval = vasprintf(strp, format, ap);
    va_end(ap);
    return retval;
}
#endif

void TIFFMessageHandlerFunc(const char *module, const char *fmt, va_list ap) {
    if(fmt && *fmt) {
        char* msg(nullptr);
        vasprintf(&msg, fmt, ap);
        if(msg) {
            SLIDEIO_LOG(WARNING) << "TIFF Warning:" << msg;
            free(msg);
        }
    }
}

bool IsFatalError(const char* module, char* msg) {
    if (module && strcmp(module,"TIFFSetField")==0 && 
        (msg && strstr(msg, "Unknown pseudo-tag") !=nullptr)) {
        return false;
    }
    return true;
}    

void TIFFErrorHandlerFunc(const char *module, const char *fmt, va_list ap) {
    if (fmt && *fmt) {
        char* msg(nullptr);
        vasprintf(&msg, fmt, ap);
        if (msg) {
            if (IsFatalError(module, msg)) {
                SLIDEIO_LOG(ERROR) << "TIFF Error:" << msg;
            } else {
                SLIDEIO_LOG(WARNING) << "TIFF Error:" << msg;
            }
            free(msg);
        }
    }
}

TIFFMessageHandler::TIFFMessageHandler() {
    m_oldErrorHandler = (void*)TIFFSetErrorHandler(TIFFErrorHandlerFunc);
    m_oldWarningHandler = (void*)TIFFSetWarningHandler(TIFFMessageHandlerFunc);
}

TIFFMessageHandler::~TIFFMessageHandler() {
    TIFFSetErrorHandler((TIFFErrorHandler)m_oldErrorHandler);
    TIFFSetWarningHandler((TIFFErrorHandler)m_oldWarningHandler);
}
