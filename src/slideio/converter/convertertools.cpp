// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/converter/convertertools.hpp"

#include "slideio/base/exceptions.hpp"


int slideio::ConverterTools::computeNumZoomLevels(int width, int height)
{
    int numZoomLevels = 1;
    int currentWidth(width), currentHeight(height);
    while(currentWidth > 1000 && currentHeight > 1000) {
        currentWidth /= 2;
        currentHeight /= 2;
        numZoomLevels++;
    }
    return numZoomLevels;
}

void adjustTileRectToScene(const cv::Rect& sceneRect, int zoomLevel, cv::Rect& tileRect, cv::Size& tileSize)
{
    if (!sceneRect.contains(tileRect.br())) {
    }
}

cv::Size slideio::ConverterTools::scaleSize(const cv::Size& size, int zoomLevel, bool downScale)
{
    if(zoomLevel<0) {
        RAISE_RUNTIME_ERROR << "Expected positive zoom level.";
    }
    if(downScale) {
        cv::Size newSize(size);
        newSize.width >>= zoomLevel;
        newSize.height >>= zoomLevel;
        return newSize;
    }
    cv::Size newSize(size);
    newSize.width <<= zoomLevel;
    newSize.height <<= zoomLevel;
    return newSize;
}


void slideio::ConverterTools::readTile(const CVScenePtr& scene, int zoomLevel,
    const cv::Rect& sceneBlockRect, cv::OutputArray tile)
{
    cv::Rect rectScene = scene->getRect();
    cv::Size tileSize = ConverterTools::scaleSize(sceneBlockRect.size(), zoomLevel, true);
    if (rectScene.contains(sceneBlockRect.br())) {
        // internal tiles
        scene->readResampledBlock(sceneBlockRect, tileSize, tile);
    }
    else {
        // border tiles
        tile.create(tileSize, CV_MAKE_TYPE(CV_8U, scene->getNumChannels()));
        tile.setTo(cv::Scalar(0));
        const cv::Rect adjustedRect = rectScene & sceneBlockRect;
        const cv::Size adjustedTileSize = scaleSize(adjustedRect.size(), zoomLevel, true);
        if (!adjustedRect.empty()) {
            cv::Mat adjustedTile;
            scene->readResampledBlock(adjustedRect, adjustedTileSize, adjustedTile);
            const cv::Rect roi(0, 0, adjustedTileSize.width, adjustedTileSize.height);
            cv::Mat matTile = tile.getMat();
            adjustedTile.copyTo(matTile(roi));
        }
    }
}

cv::Rect slideio::ConverterTools::scaleRect(const cv::Rect& rect, int zoomLevel, bool downScale)
{
    if (zoomLevel < 0) {
        RAISE_RUNTIME_ERROR << "Expected positive zoom level.";
    }
    if (downScale) {
        cv::Rect newRect(rect);
        newRect.width >>= zoomLevel;
        newRect.height >>= zoomLevel;
        newRect.x >>= zoomLevel;
        newRect.y >>= zoomLevel;
        return newRect;
    }
    cv::Rect newRect(rect);
    newRect.width <<= zoomLevel;
    newRect.height <<= zoomLevel;
    newRect.x <<= zoomLevel;
    newRect.y <<= zoomLevel;
    return newRect;
}

cv::Rect slideio::ConverterTools::computeZoomLevelRect(const cv::Rect& sceneRect,
                                                        const cv::Size& tileSize,
                                                        int zoomLevel)
{
    cv::Rect levelRect = scaleRect(sceneRect, zoomLevel, true);
    // align image size to integer number of tiles
    levelRect.width = ((levelRect.width - 1) / tileSize.width + 1) * tileSize.width;
    levelRect.height = ((levelRect.height - 1) / tileSize.height + 1) * tileSize.height;
    return levelRect;
}

