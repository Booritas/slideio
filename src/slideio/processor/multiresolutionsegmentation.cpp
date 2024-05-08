// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.

#include "slideio/processor/project.hpp"
#include "slideio/processor/multiresolutionsegmentation.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/processor/imageobject.hpp"
#include "slideio/processor/neighborcontainer.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/persistence/storage.hpp"
#include <opencv2/core/mat.hpp>
#include <set>
#include <map>

using namespace slideio;

double mergeScore(ImageObject* obj1, ImageObject* obj2) {
   return 0;
}

void mergeObjects(ImageObject* obj1, ImageObject* obj2) {
   return;
}

static void processTile(std::shared_ptr<ImageObjectManager>& imgObjMngr, const cv::Mat& raster, const cv::Point& org,
                        std::shared_ptr<MultiResolutionSegmentationParameters>& parameters,
                        cv::Mat& objectsMat) {
    const int tileObjects = raster.rows * raster.cols;
    const int elemSize = static_cast<int>(raster.elemSize());
    const int channels = raster.channels();
    ImageObject* obj = nullptr;
    cv::Point pnt;
    for (int y = 0; y < raster.rows; ++y) {
        int* objectsRow = objectsMat.ptr<int>(y);
        int* objectsPixel = objectsRow;
        for (int x = 0; x < raster.cols; ++x, ++objectsPixel) {
            if (*objectsPixel == 0) {
                obj = &(imgObjMngr->createObject());
                pnt.x = x;
                pnt.y = y;
                *objectsPixel = obj->m_id;
                obj->m_boundingRect = cv::Rect(org + pnt, cv::Size(1, 1));
                obj->m_innerPoint = org + pnt;
                obj->m_pixelCount = 1;
            }
        }
    }
    std::set<int32_t> processedIds;
    for (int y = 0; y < raster.rows; ++y) {
        int* objectsRow = objectsMat.ptr<int>(y);
        int* objectsPixel = objectsRow;
        std::map<ImageObject*,double> neighborScores;
        for (int x = 0; x < raster.cols; ++x, ++objectsPixel) {
            const int32_t id = *objectsPixel;
            ImageObject* obj = imgObjMngr->getObjectPtr(id);
            if(processedIds.find(id) == processedIds.end()) {
                NeighborContainer nghs(obj, imgObjMngr.get(), objectsMat, org);
                for(ImageObject* ngh:nghs) {
                    if(!ngh) {
                        continue;
                    }
                    if(processedIds.find(ngh->m_id) == processedIds.end() && neighborScores.find(ngh) == neighborScores.end()) {
                        double score = mergeScore(obj, ngh);
                        if(score >1.) {
                            neighborScores[ngh] = score;
                        }
                    }
                }
                for(auto nghScore: neighborScores) {
                    mergeObjects(obj, nghScore.first);
                }
                processedIds.insert(id);
            }
        }
    }
}

void slideio::mutliResolutionSegmentation(std::shared_ptr<Project> &project,
                                          std::shared_ptr<MultiResolutionSegmentationParameters> &parameters) {
    if (!project) {
        RAISE_RUNTIME_ERROR << "Project is not initialized";
    }

    auto scene = project->getScene();
    auto storage = project->getStorage();
    auto objects = project->getObjects();

    if (!scene) {
        RAISE_RUNTIME_ERROR << "Multi-resolution Segmentation: Scene is not initialized";
    }

    if (!storage) {
        RAISE_RUNTIME_ERROR << "Multi-resolution Segmentation: Storage is not initialized";
    }

    if (!objects) {
        RAISE_RUNTIME_ERROR << "Multi-resolution Segmentation: ImageObjectManager is not initialized";
    }

    const double scale = parameters->getScale();
    if (scale <= 0) {
        RAISE_RUNTIME_ERROR << "Multi-resolution Segmentation: Scale must be positive. Received:" << scale;
    }
    const cv::Size sceneTileSize = parameters->getTileSize();

    if (sceneTileSize.width <= 0 || sceneTileSize.height <= 0) {
        RAISE_RUNTIME_ERROR << "Multi-resolution Segmentation: Tile size must be positive. Received:" << sceneTileSize;
    }

    const cv::Size tileOverlapping = parameters->getTileOverlapping();
    if (tileOverlapping.width < 0 || tileOverlapping.height < 0) {
        RAISE_RUNTIME_ERROR << "Multi-resolution Segmentation: Tile overlapping must be non-negative. Received:"
                            << tileOverlapping;
    }
    const cv::Rect imageRect = scene->getRect();
    cv::Size imageSize = imageRect.size();
    cv::Size sceneSize;
    Tools::scaleSize(imageRect.size(), scale, scale, sceneSize);

    cv::Size sceneTileSizeOverlap = sceneTileSize + tileOverlapping;
    cv::Size imageTileSizeOverlap;
    Tools::scaleSize(sceneTileSizeOverlap, 1. / scale, 1. / scale, imageTileSizeOverlap);

    cv::Size imageTileSize;
    Tools::scaleSize(sceneTileSize, 1. / scale, 1. / scale, imageTileSize);

    const int tilesX = (sceneSize.width - 1) / sceneTileSize.width + 1;
    const int tilesY = (sceneSize.height - 1) / sceneTileSize.height + 1;

    const std::vector<int> chanelIndices = {0};
    const int channels = scene->getNumChannels();
    const std::vector<double> channelWeights(channels, 1.);

    cv::Point sceneOrg = {0, 0};
    cv::Point imageOrg = {0, 0};

    cv::Mat tile, tileObjects;
    for (int ty = 0; ty < tilesY; ++ty, sceneOrg.y += sceneTileSize.height, imageOrg.y += imageTileSize.height) {
        sceneOrg.x = 0;
        imageOrg.x = 0;
        for (int tx = 0; tx < tilesX; ++tx, sceneOrg.x += sceneTileSize.width, imageOrg.x += imageTileSize.width) {
            const cv::Size currentSceneTileSize = {std::min(sceneTileSizeOverlap.width, sceneSize.width - sceneOrg.x),
            std::min(sceneTileSizeOverlap.height, sceneSize.height - sceneOrg.y)
            };
            const cv::Size currentImageTileSize = {std::min(imageTileSizeOverlap.width, imageSize.width - imageOrg.x),
            std::min(imageTileSizeOverlap.height, imageSize.height - imageOrg.y)
            };

            const cv::Rect tileRect(sceneOrg, currentSceneTileSize);
            storage->readTile(sceneOrg, sceneTileSizeOverlap, tileObjects);
            cv::Rect imageRect(imageOrg, currentImageTileSize);
            scene->readResampledBlockChannels(imageRect, currentSceneTileSize, chanelIndices, tile);
            processTile(objects, tile, sceneOrg, parameters, tileObjects);
            storage->writeTile(tileObjects, sceneOrg);
        }
    }
}
