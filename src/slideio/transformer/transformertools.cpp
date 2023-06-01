// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "transformertools.hpp"

#include "convolutionfilter.hpp"
#include "transformation.hpp"

using namespace slideio;

int TransformerTools::getBlockInflationValue(const Transformation& transformation)
{
    TransformationType transformationType = transformation.getType();
    return 0;
}

void TransformerTools::computeInflatedRectParams(const cv::Size& sceneSize, const cv::Rect& blockRect, int inflationValue,
    const cv::Size& blockSize, cv::Rect& inflatedBlockRect, cv::Size& inflatedSize,
    cv::Point& blockPositionInInflatedRect)
{
    double xScale = (double)blockSize.width / (double)blockRect.width;
    double yScale = (double)blockSize.height / (double)blockRect.height;
    int inflationValueX = std::lround(inflationValue / xScale);
    int inflationValueY = std::lround(inflationValue / yScale);
    inflatedBlockRect = blockRect;
    inflatedBlockRect.x -= inflationValueX;
    inflatedBlockRect.y -= inflationValueY;
    inflatedBlockRect.width += 2*inflationValueX;
    inflatedBlockRect.height += 2*inflationValueY;
    inflatedBlockRect &= cv::Rect(0, 0, sceneSize.width, sceneSize.height);
    inflatedSize.width = std::lround(xScale*(double)inflatedBlockRect.width);
    inflatedSize.height = std::lround(yScale*(double)inflatedBlockRect.height);
    blockPositionInInflatedRect = cv::Point(
        std::lround(xScale*(blockRect.x - inflatedBlockRect.x)),
        std::lround(yScale*(blockRect.y - inflatedBlockRect.y)));
}

int TransformerTools::getBlockExtensionForGaussianBlur(const GaussianBlurFilter& gaussianBlur)
{
    int kernel = std::max(gaussianBlur.getKernelSizeX(), gaussianBlur.getKernelSizeY());
    if (kernel == 0) {
        kernel = (int)ceil(2 *
            ceil(2 * std::max(gaussianBlur.getSigmaX(), gaussianBlur.getSigmaY())) + 1);
    }
    const int extension = (kernel + 1) / 2;
    return extension;
}
