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

void slideio::ConverterTools::readTile(const CVScenePtr& scene,
    int zoomLevel,
    const cv::Rect& tileRect,
    const cv::Size& tileSize,
    cv::OutputArray tile)
{
    cv::Rect rectScene = scene->getRect();
    if (rectScene.contains(tileRect.br())) {
        scene->readResampledBlock(tileRect, tileSize, tile);
    }
    else {
        cv::Rect adjustedRect = rectScene & tileRect;
        cv::Size adjustedTileSize(adjustedRect.width >> zoomLevel, adjustedRect.height >> zoomLevel);
        cv::Mat adjustedTile;
        tile.create(tileSize, CV_MAKE_TYPE(CV_8U, scene->getNumChannels()));
        tile.setTo(cv::Scalar(0));
        if (!adjustedRect.empty()) {
            scene->readResampledBlock(adjustedRect, adjustedTileSize, adjustedTile);
            cv::Rect roi(0, 0, adjustedTileSize.width, adjustedTileSize.height);
            cv::Mat matTile = tile.getMat();
            adjustedTile.copyTo(matTile(roi));
        }
    }
}
