// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <cstdint>
#include "slideio/core/slideio_enums.hpp"

// The pixel function of auxfiles/nd2-fixtures/generate.py:
//     v = x + 3*y + 17*k + 31*frame + 7*salt
//     unsigned types: v mod 2^bits (bits = storage width)    float32: v + 0.5
// k is the component index within the frame, frame the ND2 frame index
// (ImageDataSeq|frame!), salt 0 in every committed file. Row padding is 0xEE.
inline double nd2Expected(int x, int y, int k, int64_t frame, int salt, slideio::DataType type)
{
    const int64_t v = int64_t(x) + 3 * int64_t(y) + 17 * int64_t(k) + 31 * frame + 7 * int64_t(salt);
    switch (type) {
    case slideio::DataType::DT_Byte: return static_cast<double>(v % 256);
    case slideio::DataType::DT_UInt16: return static_cast<double>(v % 65536);
    case slideio::DataType::DT_Float32: return static_cast<double>(v) + 0.5;
    default: return -1;
    }
}
