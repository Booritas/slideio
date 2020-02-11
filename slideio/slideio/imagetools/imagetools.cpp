// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/slideio.hpp"
#include <boost/format.hpp>
#include <boost/filesystem.hpp>



int slideio::ImageTools::dataTypeSize(slideio::DataType dt)
{
    switch(dt)
    {
    case DataType::DT_Byte:
    case DataType::DT_Int8:
        return 1;
    case DataType::DT_UInt16:
    case DataType::DT_Int16:
    case DataType::DT_Float16:
        return 2;
    case DataType::DT_Int32:
    case DataType::DT_Float32:
        return 4;
    case DataType::DT_Float64:
        return 8;
    }
    throw std::runtime_error(
        (boost::format("Unknown data type: %1%") % (int)dt).str());
}

void slideio::ImageTools::scaleRect(const cv::Rect& srcRect, const cv::Size& newSize, cv::Rect& trgRect)
{
    double scaleX = static_cast<double>(newSize.width) / static_cast<double>(srcRect.width);
    double scaleY = static_cast<double>(newSize.height) / static_cast<double>(srcRect.height);
    trgRect.x = static_cast<int>(std::floor(static_cast<double>(srcRect.x)*scaleX));
    trgRect.y = static_cast<int>(std::floor(static_cast<double>(srcRect.y)*scaleY));
    trgRect.width = newSize.width;
    trgRect.height = newSize.height;
}

void slideio::ImageTools::scaleRect(const cv::Rect& srcRect, double scaleX, double scaleY, cv::Rect& trgRect)
{
    trgRect.x = static_cast<int>(std::floor(static_cast<double>(srcRect.x)*scaleX));
    trgRect.y = static_cast<int>(std::floor(static_cast<double>(srcRect.y)*scaleY));
    int xn = srcRect.x + srcRect.width;
    int yn = srcRect.y + srcRect.height;
    int dxn = static_cast<int>(std::ceil(static_cast<double>(xn)* scaleX));
    int dyn = static_cast<int>(std::ceil(static_cast<double>(yn)* scaleY));
    trgRect.width = dxn - trgRect.x;
    trgRect.height = dyn - trgRect.y;
}