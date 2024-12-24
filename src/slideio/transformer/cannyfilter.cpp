// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "cannyfilter.hpp"
#include <opencv2/imgproc.hpp>
#include "slideio/base/slideio_enums.hpp"

using namespace slideio;

void CannyFilter::applyTransformation(const cv::Mat& block, cv::OutputArray transformedBlock) const
{
    cv::Canny(block, transformedBlock, getThreshold1(), getThreshold2(), getApertureSize(),getL2Gradient());
}

std::vector<DataType> CannyFilter::computeChannelDataTypes(const std::vector<DataType>& channels) const
{
    return {DataType::DT_Byte};
}

int CannyFilter::getInflationValue() const
{
    return (getApertureSize() + 1) / 2;
}
