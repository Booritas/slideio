// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#ifndef OPENCV_slideio_slideio_HPP
#define OPENCV_slideio_slideio_HPP

#include <opencv2/core.hpp>
#include "slideio/slideio_def.hpp"
#include "slideio/core/slide.hpp"
#include "slideio/core/structs.hpp"
#include <string>
#include <vector>

namespace  slideio
{
    SLIDEIO_EXPORTS std::shared_ptr<Slide> openSlide(const std::string& path, const std::string& driver);
    SLIDEIO_EXPORTS std::vector<cv::String> getDriverIDs();
    inline DataType fromOpencvType(int type)
    {
        return static_cast<DataType>(type);
    }
    inline int toOpencvType(DataType dt)
    {
        return static_cast<int>(dt);
    }
    inline bool isValidDataType(slideio::DataType dt)
    {
        return dt <= slideio::DataType::DT_LastValid;
    }
}

#endif