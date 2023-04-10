// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_convertertools_HPP
#define OPENCV_slideio_convertertools_HPP

#include "slideio/converter/converter_def.hpp"
#include "slideio/core/cvscene.hpp"

namespace slideio
{
    class SLIDEIO_CONVERTER_EXPORTS ConverterTools
    {
    public:
        static int computeNumZoomLevels(int width, int height);
        static cv::Size scaleSize(const cv::Size& size, int zoomLevel, bool downScale=true);
        static cv::Rect scaleRect(const cv::Rect& rect, int zoomLevel, bool downScale);
        static void readTile(const CVScenePtr& scene, int zoomLevel, const cv::Rect& sceneBlockRect,
                             int slice, int frame, cv::OutputArray tile);
        static cv::Rect computeZoomLevelRect(const cv::Rect& sceneRect, const cv::Size& tileSize, int zoomLevel);


    };
}

#endif