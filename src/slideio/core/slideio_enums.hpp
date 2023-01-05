// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <ostream>
#include <string>
#include "slideio/core/base.hpp"

namespace slideio
{
    /**@brief raster data compression enum*/
    enum class Compression
    {
        /**@brief Unknown compression.*/
        Unknown,
        /**@brief Uncompressed data.*/
        Uncompressed,
        /**@brief JPEG compression*/
        Jpeg,
        /**@brief JPEG XR compression*/
        JpegXR,
        /**@brief PNG compression*/
        Png,
        /**@brief JPEG 2000 compression*/
        Jpeg2000,
        /**@brief Lempel - Ziv - Welch universal lossless data compression algorithm.*/
        LZW,
        /**@brief RL-Huffman encoding*/
        HuffmanRL,
        /**@brief CCITT T.4 2-Dimensional compression*/
        CCITT_T4,
        /**@brief CCITT T.6 2-Dimensional compression*/
        CCITT_T6,
        /**@brief Old JPEG compression algorithm. */
        JpegOld,
        /**@brief zlib looseless data compression*/
        Zlib,
        JBIG85,
        JBIG43,
        /**@brief NeXT 2-bit RLE image compression scheme*/
        NextRLE,
        PackBits,
        ThunderScanRLE,
        RasterPadding,
        RLE_LW,
        RLE_HC,
        RLE_BL,
        PKZIP,
        KodakDCS,
        /**@brief JBIG early lossless image compression*/
        JBIG,
        NikonNEF,
        JBIG2,
        /**@brief gif image compression*/
        GIF,
        BIGGIF,
        /**@brief Run-length encoding*/
        RLE
    };
}
