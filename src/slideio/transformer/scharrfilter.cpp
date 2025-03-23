// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include <opencv2/core/mat.hpp>
#include "scharrfilter.hpp"
#include <opencv2/imgproc.hpp>
#include "slideio/core/tools/cvtools.hpp"

void slideio::ScharrFilter::applyTransformation(const cv::Mat& block, cv::OutputArray transformedBlock) const
{
    const DataType dt = getDepth();
    const int depth = CVTools::cvTypeFromDataType(dt);
    cv::Scharr(block, transformedBlock, depth, getDx(), getDy());
}

int slideio::ScharrFilter::getInflationValue() const
{
    return 2;
}

std::vector<slideio::DataType> slideio::ScharrFilter::computeChannelDataTypes(
    const std::vector<DataType>& channels) const
{
    std::vector<DataType> types(channels.size());
    for (int channel = 0; channel < channels.size(); ++channel)
        types[channel] = getDepth();
    return types;
}
