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
        DT_LastValid = 3,
        DT_Unknown = 1024,
        DT_None = 2048
    };
}

//std::ostream& operator << (std::ostream& os, const slideio::DataType& dt) {
//    switch(dt) {
//    case slideio::DataType::DT_Byte: os << "DT_Byte";  break;
//    case slideio::DataType::DT_Int8: os << "DT_Int8"; break;
//    case slideio::DataType::DT_Int16: os << "DT_Int16"; break;
//    case slideio::DataType::DT_Float16: os << "DT_Float16"; break;
//    case slideio::DataType::DT_Int32: os << "DT_Int32"; break;
//    case slideio::DataType::DT_Float32: os << "DT_Float32"; break;
//    case slideio::DataType::DT_Float64: os << "DT_Float64"; break;
//    case slideio::DataType::DT_UInt16: os << "DT_UInt16"; break;
//    case slideio::DataType::DT_Unknown: os << "DT_Unknown"; break;
//    case slideio::DataType::DT_None: os << "DT_None"; break;
//    default: os << (int)dt;
//    }
//    return os;
//}
