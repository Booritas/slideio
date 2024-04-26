// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#include "processortools.hpp"

using namespace slideio;

const cv::Point ProcessorTools::rotatePixelCW(const cv::Point& current, const cv::Point& center) {
    static cv::Point moves[3][3] =
    {
        {{0, -1}, {1, -1}, {1, 0}},
        {{-1, -1}, {0, 0}, {1, 1}},
        {{-1, 0}, {-1, 1}, {0, 1}}
    };
    cv::Point offset = current - center + cv::Point(1, 1);
    return center + moves[offset.y][offset.x];
}

bool ProcessorTools::isBorderPoint(const cv::Point &point, const cv::Mat &tile, const cv::Point &tileOffset)
{
   return false;
}

bool ProcessorTools::findFirstBorderPoint(const cv::Mat &tile, const cv::Point &tileOffset, cv::Point &borderPoint)
{
   return false;
}

cv::Point ProcessorTools::rotatePointCW(const cv::Point& point, const cv::Point& center) {
    static cv::Point neighbors[3][3] = {
            {{0, 0},  {1, 0}, {0, 0}},
            {{0, -1}, {0,  0}, {0, 1}},
            {{0, 0},  {-1,  0}, {0, 0}}
    };
    const cv::Point offset =  point - center + cv::Point(1, 1);
    const cv::Point next = center + neighbors[offset.y][offset.x];
    return next;
}
