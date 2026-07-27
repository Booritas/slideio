// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "openjpeg.h"

namespace slideio
{
    class OPJStreamUserData
    {
    public:
        OPJStreamUserData(unsigned char* ptr, size_t sz): data(ptr), size(sz), offset(0) {}
        unsigned char* data;
        size_t size;
        size_t offset;
    };
    opj_stream_t* createOPJMemoryStream(OPJStreamUserData* data, size_t size, bool inputStream);
} // namespace slideio
