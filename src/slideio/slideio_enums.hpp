// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <ostream>
#include <string>
#include "slideio/base.hpp"

namespace slideio
{
    enum class Compression
    {
        Unknown,
        Uncompressed,
        Jpeg,
        JpegXR,
        Png,
        Jpeg2000,
        LZW,
        HuffmanRL,
        CCITT_T4,
        CCITT_T6,
        LempelZivWelch,
        JpegOld,
        Zlib,
        JBIG85,
        JBIG43,
        NextRLE,
        PackBits,
        ThunderScanRLE,
        RasterPadding,
        RLE_LW,
        RLE_HC,
        RLE_BL,
        PKZIP,
        KodakDCS,
        JBIG,
        NikonNEF,
        JBIG2,
        GIF,
        BIGGIF,
        RLE
    };
}
