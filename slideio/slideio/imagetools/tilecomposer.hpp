// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_tilecomposer_HPP
#define OPENCV_slideio_tilecomposer_HPP

#include "slideio/slideio_def.hpp"
#include <opencv2/core.hpp>

namespace cv
{
    namespace slideio
    {
        class SLIDEIO_EXPORTS Tiler
        {
        public:
            virtual ~Tiler() {}
            virtual int getTileCount(void* userData) = 0;
            virtual bool getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) = 0;
            virtual bool readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster, void* userData) = 0;
        };
        class SLIDEIO_EXPORTS TileComposer
        {
        public:
            static void composeRect(Tiler* tiler, const std::vector<int>& channelIndices,
                const cv::Rect& blockRect, const cv::Size& blockSize, cv::OutputArray output, void* userData = nullptr);
        };
    }
}
#endif