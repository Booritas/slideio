// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include <vector>
#include <opencv2/core/mat.hpp>

#include "slideio/converter/converter_def.hpp"
#include "slideio/core/size.hpp"
#include "slideio/core/rect.hpp"

namespace slideio
{
    class CVScene;
    namespace converter
    {
        class ConverterParameters;
        class SLIDEIO_CONVERTER_EXPORTS ConverterTools
        {
        public:
            static int computeNumZoomLevels(int width, int height);
            static Size scaleSize(const Size& size, int zoomLevel, bool downScale = true);
            static Rect scaleRect(const Rect& rect, int zoomLevel, bool downScale);
            static void readTile(const std::shared_ptr <CVScene>& scene, const std::vector<int> channels, int zoomLevel, const cv::Rect& sceneBlockRect,
                int slice, int frame, cv::OutputArray tile);
            static Rect computeZoomLevelRect(const Rect& sceneRect, const Size& tileSize, int zoomLevel);
			static int computeNumTiles(const Size& imageSize, const Size& tileSize) {
                const int numTilesX = (imageSize.width + tileSize.width - 1) / tileSize.width;
                const int numTilesY = (imageSize.height + tileSize.height - 1) / tileSize.height;
                return numTilesX * numTilesY;
            }

        };
    }
}
