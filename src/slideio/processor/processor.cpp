// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#include "processor.hpp"

#include "slideio/base/exceptions.hpp"

using namespace slideio;

static void processTile(const cv::Mat& raster, double colorThreshold, double compactnessThreshold, cv::OutputArray objects) {
    
}

void Processor::multiResolutionSegmentation(std::shared_ptr<CVScene> scene, int channelIndex, double colorThreshold,
                                            double compactnessThreshold, std::shared_ptr<Storage> storage) {
    if (!scene) {
        RAISE_RUNTIME_ERROR << "Scene is not initialized";
    }
    if (!storage) {
        RAISE_RUNTIME_ERROR << "Storage is not initialized";
    }
    const cv::Size tileSize(512, 512);
    const cv::Rect sceneRect = scene->getRect();
    const cv::Size sceneSize = sceneRect.size();
    const int tilesX = (sceneSize.width - 1) / tileSize.width + 1;
    const int tilesY = (sceneSize.height - 1) / tileSize.height + 1;
    const std::vector<int> chanelIndices = { channelIndex };
    int y = 0;
    cv::Mat tile, tileObjects;
    for (int ty = 0; ty < tilesY; ++ty, y+=tileSize.height) {
        int x = 0;
        for (int tx = 0; tx < tilesX; ++tx, x+=tileSize.width) {
            const int tileWidth = std::min(tileSize.width, sceneSize.width - x);
            const int tileHeight = std::min(tileSize.height, sceneSize.height - y);
            const cv::Rect tileRect(x, y, tileWidth, tileHeight);
            scene->readBlockChannels(tileRect, chanelIndices, tile);
            processTile(tile, colorThreshold, compactnessThreshold, tileObjects);
            //storage->writeTile(tileObjects, cv::Point(x,y));
        }
    }
}
