// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <opencv2/imgproc.hpp>
#include "sobelfilter.hpp"
#include "slideio/core/tools/cvtools.hpp"

using namespace slideio;

void SobelFilter::applyTransformation(const cv::Mat& block, cv::OutputArray transformedBlock) const
{
    DataType dt = getDepth();
    const int depth = CVTools::cvTypeFromDataType(dt);
    cv::Sobel(block, transformedBlock, depth, m_dx, m_dy, m_ksize, m_scale, m_delta);
}

int SobelFilter::getInflationValue() const
{
    return (getKernelSize() + 1) / 2;
}

std::vector<DataType> SobelFilter::computeChannelDataTypes(
    const std::vector<DataType>& channels) const
{
    std::vector<DataType> types(channels.size());
    for(int channel = 0; channel < channels.size(); ++channel)
        types[channel] = getDepth();
    return types;
}
