// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#include "processor.hpp"

#include "imageobjectpixeliterator.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/imagetools/cvtools.hpp"
#include "slideio/processor/imageobjectmanager.hpp"
#include "slideio/persistence/storage.hpp"
#include "slideio/processor/project.hpp"

using namespace slideio;

class MergeData
{
    double sqrSum;
    double sum;
};

template <typename type>
void computeSums(type* pixels, int channels, std::vector<double>weights, double& sum, double& squareSum) {
    for (int c = 0; c < channels; ++c) {
        const double val = pixels[c] * weights[c];
        sum += val;
        squareSum += val * val;
    }
}

void computePixelMergeData(const cv::Mat& raster, const cv::Point& point, const std::vector<double>& channelWeights, double& sum, double& squareSum) {
    const uint8_t* rasterRow = raster.ptr<uint8_t>(point.y);
    const uint8_t* rasterPixel = rasterRow + point.x * raster.elemSize();
    const int dataType = raster.depth();
    switch (dataType) {
    case CV_8U:
        computeSums(rasterPixel, raster.channels(), channelWeights, sum, squareSum);
        break;
    case CV_8S:
        computeSums((int8_t*)rasterPixel, raster.channels(), channelWeights, sum, squareSum);
        break;
    case CV_16U:
        computeSums((uint16_t*)rasterPixel, raster.channels(), channelWeights, sum, squareSum);
        break;
    case CV_16S:
        computeSums((int16_t*)rasterPixel, raster.channels(), channelWeights, sum, squareSum);
        break;
    case CV_32S:
        computeSums((int32_t*)rasterPixel, raster.channels(), channelWeights, sum, squareSum);
        break;
    case CV_16F:
        computeSums((cv::float16_t*)rasterPixel, raster.channels(), channelWeights, sum, squareSum);
        break;
    case CV_32F:
        computeSums((float*)rasterPixel, raster.channels(), channelWeights, sum, squareSum);
        break;
    case CV_64F:
        computeSums((double*)rasterPixel, raster.channels(), channelWeights, sum, squareSum);
        break;
    default:
        RAISE_RUNTIME_ERROR << "Segmentation: Unsupported data type:" << dataType;
    }
}

void computeMergeData(ImageObject* obj, const cv::Mat& raster, cv::Mat& mat, const cv::Point& point, const std::vector<double>& weights, MergeData& mergeData) {
    double sum = 0.;
    double squareSum = 0.;
    ImageObjectPixelContainer pixels(obj, mat, cv::Rect(point,cv::Size(mat.cols, mat.rows)));
    for(const cv::Point& pixel : pixels) {
        computePixelMergeData(raster, pixel, weights, sum, squareSum);
    }
}

static void processTile(std::shared_ptr<ImageObjectManager>& imgObjMngr, const cv::Mat& raster, const cv::Point& org,
                        double colorThreshold, double compactnessThreshold, const std::vector<double>& channelWeights,
                        cv::OutputArray objects) {
    const int tileObjects = raster.rows * raster.cols;
    objects.create(tileObjects, 1, CV_32SC1);
    cv::Mat objectsMat = objects.getMat();
    const int elemSize = static_cast<int>(raster.elemSize());
    const int channels = raster.channels();
    ImageObject* obj = nullptr;
    cv::Point pnt;
    for (int y = 0; y < raster.rows && obj == nullptr; ++y) {
        int* objectsRow = objectsMat.ptr<int>(y);
        int* objectsPixel = objectsRow;
        for (int x = 0; x < raster.cols && obj == nullptr; ++x, ++objectsPixel) {
            if (*objectsPixel == 0) {
                obj = &(imgObjMngr->createObject());
                pnt.x = x;
                pnt.y = y;
                *objectsPixel = obj->m_id;
            }
        }
    }
    std::map<int, MergeData> mergeData;
    if (obj != nullptr) {
        std::deque<cv::Point> queue;
        queue.push_back(pnt);
        while (!queue.empty()) {
            const cv::Point curPoint = queue.front();
            queue.pop_front();
            if (mergeData.find(obj->m_id) == mergeData.end()) {
                mergeData[obj->m_id] = MergeData();
                computeMergeData(obj, raster, objectsMat, curPoint,channelWeights, mergeData[obj->m_id]);
            }
            //ImageObject& obj = imgObjMngr->getObject(id);
        }
    }
}

void Processor::multiResolutionSegmentation(std::shared_ptr<Project> project, int channelIndex, double colorThreshold,
                                            double compactnessThreshold) {
    if (!project) {
        RAISE_RUNTIME_ERROR << "Project is not initialized";
    }
    auto scene = project->getScene();
    auto storage = project->getStorage();
    auto objects = project->getObjects();

    if (!scene) {
        RAISE_RUNTIME_ERROR << "Scene is not initialized";
    }
    if (!storage) {
        RAISE_RUNTIME_ERROR << "Storage is not initialized";
    }
    if (!objects) {
        RAISE_RUNTIME_ERROR << "ImageObjectManager is not initialized";
    }
    const cv::Size tileSize(512, 512);
    const cv::Rect sceneRect = scene->getRect();
    const cv::Size sceneSize = sceneRect.size();
    const int tilesX = (sceneSize.width - 1) / tileSize.width + 1;
    const int tilesY = (sceneSize.height - 1) / tileSize.height + 1;
    const std::vector<int> chanelIndices = {channelIndex};
    const int channels = scene->getNumChannels();
    const std::vector<double> channelWeights(channels, 1.);
    int y = 0;
    cv::Mat tile, tileObjects;
    for (int ty = 0; ty < tilesY; ++ty, y += tileSize.height) {
        int x = 0;
        for (int tx = 0; tx < tilesX; ++tx, x += tileSize.width) {
            const int tileWidth = std::min(tileSize.width, sceneSize.width - x);
            const int tileHeight = std::min(tileSize.height, sceneSize.height - y);
            const cv::Rect tileRect(x, y, tileWidth, tileHeight);
            scene->readBlockChannels(tileRect, chanelIndices, tile);
            processTile(objects, tile, tileRect.tl(), colorThreshold, compactnessThreshold, channelWeights,
                        tileObjects);
            //storage->writeTile(tileObjects, cv::Point(x,y));
        }
    }
}
