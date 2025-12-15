// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/converter/convertertools.hpp"

#include "slideio/base/exceptions.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include "slideio/converter/converterparameters.hpp"

using namespace slideio;
using namespace slideio::converter;

int ConverterTools::computeNumZoomLevels(int width, int height) {
    int numZoomLevels = 1;
    int currentWidth(width), currentHeight(height);
    while (currentWidth > 1000 && currentHeight > 1000) {
        currentWidth /= 2;
        currentHeight /= 2;
        numZoomLevels++;
    }
    return numZoomLevels;
}

cv::Size ConverterTools::scaleSize(const cv::Size& size, int zoomLevel, bool downScale) {
    if (zoomLevel < 0) {
        RAISE_RUNTIME_ERROR << "Expected positive zoom level.";
    }
    if (downScale) {
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


void ConverterTools::readTile(const CVScenePtr& scene, const std::vector<int> channels, int zoomLevel,
                              const cv::Rect& sceneBlockRect, int slice, int frame, cv::OutputArray tile) {
    cv::Range slices(slice, slice + 1);
    cv::Range frames(frame, frame + 1);
    cv::Rect rectScene = scene->getRect();
    rectScene.x = rectScene.y = 0;
    cv::Size tileSize = ConverterTools::scaleSize(sceneBlockRect.size(), zoomLevel, true);
    if (rectScene.contains(sceneBlockRect.br())) {
        // internal tiles
        scene->readResampled4DBlockChannels(sceneBlockRect, tileSize, channels, slices, frames, tile);
    }
    else {
        // border tiles
        int dt = CVTools::toOpencvType(scene->getChannelDataType(0));
        tile.create(tileSize, CV_MAKE_TYPE(dt, scene->getNumChannels()));
        tile.setTo(0);
        const cv::Rect adjustedRect = rectScene & sceneBlockRect;
        const cv::Size adjustedTileSize = scaleSize(adjustedRect.size(), zoomLevel, true);
        if (!adjustedRect.empty()) {
            cv::Mat adjustedTile;
            scene->readResampled4DBlockChannels(adjustedRect, adjustedTileSize, channels, slices, frames, adjustedTile);
            if (!adjustedTile.empty()) {
                const cv::Rect roi(0, 0, adjustedTileSize.width, adjustedTileSize.height);
                cv::Mat matTile = tile.getMat();
                adjustedTile.copyTo(matTile(roi));
            }
        }
    }
}

cv::Rect ConverterTools::scaleRect(const cv::Rect& rect, int zoomLevel, bool downScale) {
    if (zoomLevel < 0) {
        RAISE_RUNTIME_ERROR << "Expected positive zoom level.";
    }
    cv::Rect newRect(rect);
    if (downScale) {
        newRect.width >>= zoomLevel;
        newRect.height >>= zoomLevel;
        newRect.x >>= zoomLevel;
        newRect.y >>= zoomLevel;
        return newRect;
    }
    newRect.width <<= zoomLevel;
    newRect.height <<= zoomLevel;
    newRect.x <<= zoomLevel;
    newRect.y <<= zoomLevel;
    return newRect;
}

cv::Rect ConverterTools::computeZoomLevelRect(const cv::Rect& sceneRect,
                                              const cv::Size& tileSize,
                                              int zoomLevel) {
    cv::Rect levelRect = scaleRect(sceneRect, zoomLevel, true);
    // align image size to integer number of tiles
    levelRect.width = ((levelRect.width - 1) / tileSize.width + 1) * tileSize.width;
    levelRect.height = ((levelRect.height - 1) / tileSize.height + 1) * tileSize.height;
    return levelRect;
}
