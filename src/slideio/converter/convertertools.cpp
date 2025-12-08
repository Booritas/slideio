// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/converter/convertertools.hpp"

#include "convertersvstools.hpp"
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

void adjustTileRectToScene(const cv::Rect& sceneRect, int zoomLevel, cv::Rect& tileRect, cv::Size& tileSize) {
    if (!sceneRect.contains(tileRect.br())) {
    }
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


void ConverterTools::readTile(const CVScenePtr& scene, int zoomLevel,
                              const cv::Rect& sceneBlockRect, int slice, int frame, cv::OutputArray tile) {
    cv::Range slices(slice, slice + 1);
    cv::Range frames(frame, frame + 1);
    cv::Rect rectScene = scene->getRect();
    rectScene.x = rectScene.y = 0;
    cv::Size tileSize = ConverterTools::scaleSize(sceneBlockRect.size(), zoomLevel, true);
    if (rectScene.contains(sceneBlockRect.br())) {
        // internal tiles
        scene->readResampled4DBlockChannels(sceneBlockRect, tileSize, {}, slices, frames, tile);
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
            scene->readResampled4DBlockChannels(adjustedRect, adjustedTileSize, {}, slices, frames, adjustedTile);
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

cv::Rect ConverterTools::computeZoomLevelRect(const cv::Rect& sceneRect,
                                              const cv::Size& tileSize,
                                              int zoomLevel) {
    cv::Rect levelRect = scaleRect(sceneRect, zoomLevel, true);
    // align image size to integer number of tiles
    levelRect.width = ((levelRect.width - 1) / tileSize.width + 1) * tileSize.width;
    levelRect.height = ((levelRect.height - 1) / tileSize.height + 1) * tileSize.height;
    return levelRect;
}

void ConverterTools::checkJpegRequirements(const std::shared_ptr<slideio::CVScene>& scene,
                                           const converter::ConverterParameters& parameters) {
    if (parameters.getEncoding() == Compression::Jpeg) {
        const int numChannels = scene->getNumChannels();
        if (numChannels != 1 && numChannels != 3) {
            RAISE_RUNTIME_ERROR << "Converter: Jpeg compression can be used for 1 and 3 channel images only!";
        }
        for (int channel = 0; channel < numChannels; ++channel) {
            if (scene->getChannelDataType(channel) != DataType::DT_Byte) {
                RAISE_RUNTIME_ERROR << "Converter: Jpeg compression can be used for 8bit images only!";
            }
        }
    }
}

void ConverterTools::checkEncodingRequirements(const std::shared_ptr<slideio::CVScene>& scene,
    const converter::ConverterParameters& parameters) {
    if (parameters.getEncoding() == Compression::Jpeg) {
		checkJpegRequirements(scene, parameters);
    }
}

void ConverterTools::checkContainerRequirements(const std::shared_ptr<slideio::CVScene>& scene,
    const converter::ConverterParameters& parameters) {
    if (parameters.getContainerType()==ImageFormat::SVS) {
        ConverterSVSTools::checkSVSRequirements(scene, parameters);
	}
}
