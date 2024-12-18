// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include <opencv2/core/mat.hpp>
#include "laplacianfilter.hpp"
#include <opencv2/imgproc.hpp>
#include "slideio/core/tools/cvtools.hpp"

using namespace slideio;

void LaplacianFilter::applyTransformation(const cv::Mat& block, cv::OutputArray transformedBlock) const
{
    const DataType dt = getDepth();
    const int depth = CVTools::cvTypeFromDataType(dt);
    cv::Laplacian(block, transformedBlock, depth, getKernelSize(), getScale(), getDelta());
}

int LaplacianFilter::getInflationValue() const
{
    return (getKernelSize() + 1) / 2;
}

std::vector<DataType> LaplacianFilter::computeChannelDataTypes(const std::vector<DataType>& channels) const
{
    std::vector<DataType> types(channels.size());
    for (int channel = 0; channel < channels.size(); ++channel)
        types[channel] = getDepth();
    return types;
}

