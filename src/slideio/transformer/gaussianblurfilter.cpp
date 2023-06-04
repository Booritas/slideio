// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <opencv2/imgproc.hpp>
#include "gaussianblurfilter.hpp"

using namespace slideio;

void GaussianBlurFilter::applyTransformation(const cv::Mat& block, cv::OutputArray transformedBlock) const
{
    cv::GaussianBlur(block, transformedBlock, cv::Size(getKernelSizeX(), getKernelSizeY()), getSigmaX(), getSigmaY());
}

int GaussianBlurFilter::getInflationValue() const
{
    int kernel = std::max(getKernelSizeX(), getKernelSizeY());
    if (kernel == 0) {
        kernel = (int)ceil(2 *
            ceil(2 * std::max(getSigmaX(), getSigmaY())) + 1);
    }
    const int extension = (kernel + 1) / 2;
    return extension;
}
