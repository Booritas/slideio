// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#include "processor.hpp"

#include "imageobjectpixelcontainer.hpp"
#include "imageobjectpixeliterator.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/imagetools/cvtools.hpp"
#include "slideio/processor/imageobjectmanager.hpp"
#include "slideio/persistence/storage.hpp"
#include "slideio/processor/project.hpp"

using namespace slideio;

class MergeData
{
public:
    double sqrSum;
    double sum;
    std::vector<cv::Point> contour;
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

void computeMergeData(ImageObject* obj, const cv::Mat& raster, cv::Mat& mat, const cv::Point& point, const std::vector<double>& weights, std::shared_ptr<MergeData>& mergeData) {
    double sum = 0.;
    double squareSum = 0.;
    ImageObjectPixelContainer pixels(obj, mat, cv::Rect(point,cv::Size(mat.cols, mat.rows)));
    for(const cv::Point& pixel : pixels) {
        computePixelMergeData(raster, pixel, weights, sum, squareSum);
    }
    mergeData->sqrSum = squareSum;
    mergeData->sum = sum;
}
