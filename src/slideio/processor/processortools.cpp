// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#include "processortools.hpp"

const cv::Point slideio::ProcessorTools::nextMoveCW(const cv::Point& current, const cv::Point& center) {
    static cv::Point moves[3][3] =
    {
        {{0, -1}, {1, -1}, {1, 0}},
        {{-1, -1}, {0, 0}, {1, 1}},
        {{-1, 0}, {-1, 1}, {0, 1}}
    };
    cv::Point offset = current - center + cv::Point(1, 1);
    return center + moves[offset.y][offset.x];
}

const bool slideio::ProcessorTools::isBorderPoint(const cv::Point &point, const cv::Mat &tile, const cv::Point &tileOffset)
{
   return false;
}

const bool slideio::ProcessorTools::findFirstBorderPoint(const cv::Mat &tile, const cv::Point &tileOffset, cv::Point &borderPoint)
{
   return false;
}
