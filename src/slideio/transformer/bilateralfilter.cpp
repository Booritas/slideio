// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <opencv2/core/mat.hpp>
#include "bilateralfilter.hpp"
#include <opencv2/imgproc.hpp>

using namespace slideio;

void BilateralFilter::applyTransformation(const cv::Mat& block, cv::OutputArray transformedBlock) const
{
    cv::bilateralFilter(block, transformedBlock, getDiameter(), getSigmaColor(), getSigmaSpace());
}

std::vector<DataType> BilateralFilter::computeChannelDataTypes(const std::vector<DataType>& channels) const
{
    return Transformation::computeChannelDataTypes(channels);
}

int BilateralFilter::getInflationValue() const
{
    const int d = getDiameter();
    return d > 0 ? ((d + 1) / 2) : 7;
}
