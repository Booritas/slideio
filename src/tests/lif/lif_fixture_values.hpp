// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <cstdint>
#include "slideio/core/slideio_enums.hpp"

// The pixel function of auxfiles/lif-fixtures/generate.py:
//     v = x + 3*y + 17*c + 31*z + 59*t + 97*m + 7*salt
//     unsigned types: v mod 2^bits (bits = storage width)    float types: v + 0.5
// c is the stored channel index (description order), m the tile field, salt the
// image's 0-based index in its file, in document order.
inline double lifExpected(int x, int y, int c, int z, int t, int m, int salt, slideio::DataType type)
{
    const int64_t v = int64_t(x) + 3 * int64_t(y) + 17 * int64_t(c) + 31 * int64_t(z)
        + 59 * int64_t(t) + 97 * int64_t(m) + 7 * int64_t(salt);
    switch (type) {
    case slideio::DataType::DT_Byte: return static_cast<double>(v % 256);
    case slideio::DataType::DT_UInt16: return static_cast<double>(v % 65536);
    case slideio::DataType::DT_UInt32: return static_cast<double>(v % (int64_t(1) << 32));
    case slideio::DataType::DT_Float16:
    case slideio::DataType::DT_Float32:
    case slideio::DataType::DT_Float64: return static_cast<double>(v) + 0.5;
    default: return -1;
    }
}
