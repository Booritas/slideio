// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_convertertools_HPP
#define OPENCV_slideio_convertertools_HPP

#include "slideio/converter/converter_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"

namespace slideio
{
    class SLIDEIO_CONVERTER_EXPORTS ConverterTools
    {
    public:
        static int computeNumZoomLevels(int width, int height);
        static cv::Size scaleSize(const cv::Size& size, int zoomLevel, bool downScale=true);
        static cv::Rect scaleRect(const cv::Rect& rect, int zoomLevel, bool downScale);
        static void readTile(const CVScenePtr& scene, int zoomLevel, const cv::Rect& sceneBlockRect,
                             cv::OutputArray tile);
        static cv::Rect computeZoomLevelRect(const cv::Rect& sceneRect, const cv::Size& tileSize, int zoomLevel);

        template <typename Type>
        static void convertTo32bitChannels(Type* data, int width, int height, int numChannels, int32_t** channels)
        {
            const int pixelSize = numChannels;
            const int stride = pixelSize * width;
            Type* line = data;
            int channelShift = 0;
            for (int y = 0; y < height; ++y) {
                Type* pixel = line;
                for (int x = 0; x < width; ++x) {
                    for (int channelIndex = 0; channelIndex < numChannels; ++channelIndex) {
                        int32_t* channel = channels[channelIndex];
                        channel[channelShift] = static_cast<int32_t>(pixel[channelIndex]);
                    }
                    pixel += pixelSize;
                    channelShift++;
                }
                line += stride;
            }
        }

    };
}

#endif