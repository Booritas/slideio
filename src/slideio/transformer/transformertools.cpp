// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "transformertools.hpp"
#include "transformation.hpp"

using namespace slideio;


void TransformerTools::computeInflatedRectParams(const cv::Size& sceneSize, const cv::Rect& blockRect,
                                                 int inflationValue,
                                                 const cv::Size& blockSize, cv::Rect& inflatedBlockRect,
                                                 cv::Size& inflatedSize,
                                                 cv::Point& blockPositionInInflatedRect)
{
    double xScale = (double)blockSize.width / (double)blockRect.width;
    double yScale = (double)blockSize.height / (double)blockRect.height;
    int inflationValueX = std::lround(inflationValue / xScale);
    int inflationValueY = std::lround(inflationValue / yScale);
    inflatedBlockRect = blockRect;
    inflatedBlockRect.x -= inflationValueX;
    inflatedBlockRect.y -= inflationValueY;
    inflatedBlockRect.width += 2 * inflationValueX;
    inflatedBlockRect.height += 2 * inflationValueY;
    inflatedBlockRect &= cv::Rect(0, 0, sceneSize.width, sceneSize.height);
    inflatedSize.width = std::lround(xScale * (double)inflatedBlockRect.width);
    inflatedSize.height = std::lround(yScale * (double)inflatedBlockRect.height);
    blockPositionInInflatedRect = cv::Point(
        std::lround(xScale * (blockRect.x - inflatedBlockRect.x)),
        std::lround(yScale * (blockRect.y - inflatedBlockRect.y)));
}
