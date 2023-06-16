// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include <opencv2/imgproc.hpp>
#include "colortransformation.hpp"
#include "slideio/base/exceptions.hpp"

using namespace slideio;

void ColorTransformation::applyTransformation(const cv::Mat& block, cv::OutputArray converted) const
{
    ColorSpace targetColorSpace = getColorSpace();
    switch (targetColorSpace) {
    case ColorSpace::GRAY:
        cv::cvtColor(block, converted, cv::COLOR_RGB2GRAY);
        break;
    case ColorSpace::HSV:
        cv::cvtColor(block, converted, cv::COLOR_RGB2HSV);
        break;
    case ColorSpace::HLS:
        cv::cvtColor(block, converted, cv::COLOR_RGB2HLS);
        break;
    case ColorSpace::YUV:
        cv::cvtColor(block, converted, cv::COLOR_RGB2YUV);
        break;
    case ColorSpace::YCbCr:
        cv::cvtColor(block, converted, cv::COLOR_RGB2YCrCb);
        break;
    case ColorSpace::XYZ:
        cv::cvtColor(block, converted, cv::COLOR_RGB2XYZ);
        break;
    case ColorSpace::LAB:
        cv::cvtColor(block, converted, cv::COLOR_RGB2Lab);
        break;
    case ColorSpace::LUV:
        cv::cvtColor(block, converted, cv::COLOR_RGB2Luv);
        break;
    default:
        RAISE_RUNTIME_ERROR << "Unsupported color space";
    }
}


std::vector<slideio::DataType> slideio::ColorTransformation::computeChannelDataTypes(
    const std::vector<DataType>& channels) const
{
    if(channels.empty())
    {
        RAISE_RUNTIME_ERROR << "Empty channel vector supplied for the color transformation.";
    }
    if(getColorSpace()==ColorSpace::GRAY)
    {
        return { channels[0]};
    }
    return Transformation::computeChannelDataTypes(channels);
}
