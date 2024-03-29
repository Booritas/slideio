// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "slideio/core/tools/tilecomposer.hpp"
#include "slideio/core/tools/tools.hpp"
#include <opencv2/imgproc.hpp>



void slideio::TileComposer::composeRect(slideio::Tiler* tiler,
                                        const std::vector<int>& channelIndices,
                                        const cv::Rect& blockRect,
                                        const cv::Size& blockSize,
                                        cv::OutputArray output,
                                        void *userData)
{
    const bool tileTest = false;
    const int tileCount = tiler->getTileCount(userData);
    const int channelCount = static_cast<int>(channelIndices.size());
    const cv::Point blockOrigin = blockRect.tl();
    const double scaleX = static_cast<double>(blockSize.width)/static_cast<double>(blockRect.width);
    const double scaleY = static_cast<double>(blockSize.height)/static_cast<double>(blockRect.height);
    cv::Rect scaledBlockRect;
    slideio::Tools::scaleRect(blockRect, blockSize, scaledBlockRect);
    tiler->initializeBlock(blockSize, channelIndices, output);
    cv::Mat scaledBlockRaster = output.getMat();
    for(int tileIndex = 0; tileIndex<tileCount; tileIndex++)
    {
        cv::Rect tileRect;
        tiler->getTileRect(tileIndex, tileRect, userData);
        cv::Rect intersection = blockRect & tileRect;
        if(intersection.area()>0)
        {
            cv::Mat tileRaster;
            if(tileTest)
            {
                tileRaster.create(tileRect.size(), CV_MAKETYPE(CV_8U,1));
                cv::rectangle(tileRaster, cv::Point(0, 0), cv::Point(tileRect.width-1, tileRect.height-1), 0, cv::LINE_4);
            }
            else
            {
                if(!tiler->readTile(tileIndex, channelIndices, tileRaster, userData))
                {
                    // fill tile with background color if the tile is not available
                    tiler->initializeBlock(tileRect.size(), channelIndices, tileRaster);
                }
            }
            if(!tileRaster.empty())
            {
                cv::Rect scaledTileRect;
                slideio::Tools::scaleRect(tileRect, scaleX, scaleY, scaledTileRect);
                // scale tile raster
                cv::Mat scaledTileRaster;
                cv::resize(tileRaster, scaledTileRaster, scaledTileRect.size());
                // compute intersection of scaled tile rectangle and scaled block rectangle
                cv::Rect scaledIntersectionRect = scaledBlockRect & scaledTileRect;
                if(!scaledIntersectionRect.empty()) {
                    const cv::Rect blockPart = scaledIntersectionRect - scaledBlockRect.tl();
                    const cv::Rect tilePart = scaledIntersectionRect - scaledTileRect.tl();
                    cv::Mat blockPartRaster(scaledBlockRaster, blockPart);
                    cv::Mat tilePartRaster(scaledTileRaster, tilePart);
                    tilePartRaster.copyTo(blockPartRaster);
                }
            }
        }
    }
}
