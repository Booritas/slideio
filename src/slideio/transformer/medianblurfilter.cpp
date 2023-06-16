// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include <opencv2/imgproc.hpp>
#include "medianblurfilter.hpp"

void slideio::MedianBlurFilter::applyTransformation(const cv::Mat& block, cv::OutputArray transformedBlock) const
{
    cv::medianBlur(block, transformedBlock, m_kernelSize);
}

int slideio::MedianBlurFilter::getInflationValue() const
{
    return (getKernelSize() + 1) / 2;
}
