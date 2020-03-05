// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

namespace slideio
{
    enum class DataType
    {
        DT_Byte = 0,
        DT_Int8 = 1,
        DT_Int16 = 3,
        DT_Float16 = 7,
        DT_Int32 = 4,
        DT_Float32 = 5,
        DT_Float64 = 6,
        DT_UInt16 = 2,
        DT_LastValid = 2,
        DT_Unknown = 1024,
        DT_None = 2048
    };
}