// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_structs_HPP
#define OPENCV_slideio_structs_HPP

#include <opencv2/core.hpp>

namespace slideio
{
    enum class DataType
    {
        DT_Byte = CV_8U,
        DT_Int8 = CV_8S,
        DT_Int16 = CV_16S,
        DT_Float16 = CV_16F,
        DT_Int32 = CV_32S,
        DT_Float32 = CV_32F,
        DT_Float64 = CV_64F,
        DT_UInt16 = CV_16U,
        DT_LastValid = CV_16U,
        DT_Unknown = 1024,
        DT_None = 2048
    };
    typedef cv::Point2d Resolution;
}

#endif