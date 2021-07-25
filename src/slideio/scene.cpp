// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/scene.hpp"

#include "core/cvtools.hpp"
#include "slideio/core/cvscene.hpp"
#include <boost/format.hpp>

using namespace slideio;

inline  cv::Rect tupleToRect(const std::tuple<int, int, int, int>& tpl)
{
    cv::Rect rect(std::get<0>(tpl), std::get<1>(tpl), std::get<2>(tpl), std::get<3>(tpl));
    return rect;
}

inline  cv::Size tupleToSize(const std::tuple<int, int>& tpl)
{
    cv::Size size(std::get<0>(tpl), std::get<1>(tpl));
    return size;
}

inline  cv::Range tupleToRange(const std::tuple<int, int>& tpl)
{
    cv::Range range(std::get<0>(tpl), std::get<1>(tpl));
    return range;
}

Scene::Scene(std::shared_ptr<CVScene> scene) : m_scene(scene)
{
}

std::string Scene::getFilePath() const
{
    return m_scene->getFilePath();
}

std::string Scene::getName() const
{
    return m_scene->getName();
}

std::tuple<int, int, int, int> Scene::getRect() const
{
    cv::Rect rect = m_scene->getRect();
    std::tuple<int,int,int,int> wrapper(rect.x, rect.y, rect.width, rect.height);
    return wrapper;
}

int Scene::getNumChannels() const
{
    return m_scene->getNumChannels();
}

int Scene::getNumZSlices()
{
    return m_scene->getNumZSlices();
}

int Scene::getNumTFrames()
{
    return m_scene->getNumTFrames();
}

Compression Scene::getCompression() const
{
    return m_scene->getCompression();
}

slideio::DataType Scene::getChannelDataType(int channel) const
{
    return m_scene->getChannelDataType(channel);
}

std::string Scene::getChannelName(int channel) const
{
    return m_scene->getChannelName(channel);
}

std::tuple<double, double> Scene::getResolution() const
{
    slideio::Resolution res = m_scene->getResolution();
    std::tuple<double,double> wrapper(res.x, res.y);
    return wrapper;
}

double Scene::getZSliceResolution() const
{
    return m_scene->getZSliceResolution();
}

double Scene::getTFrameResolution() const
{
    return m_scene->getTFrameResolution();
}

double Scene::getMagnification() const
{
    return m_scene->getMagnification();
}

int Scene::getBlockSize(const std::tuple<int, int>& blockSize, int refChannel, int numChannels, int numSlices, int numFrames) const
{
    const int blockWidth = std::get<0>(blockSize);
    const int blockHeight = std::get<1>(blockSize);
    const int numPlanes = numSlices*numFrames*numChannels;
    const DataType dt = m_scene->getChannelDataType(refChannel);
    const int ds = CVTools::cvGetDataTypeSize(dt);
    const int planeSize = blockWidth*blockHeight*ds;
    return planeSize*numPlanes;
}

void Scene::readBlock(const std::tuple<int, int, int, int>& rect, void* buffer, size_t bufferSize)
{
    const std::vector<int> channelIndices;
    return readBlockChannels(rect, channelIndices, buffer, bufferSize);
}

void Scene::readBlockChannels(const std::tuple<int, int, int, int>& blockRect, const std::vector<int>& channelIndices,
    void* buffer, size_t bufferSize)
{
    const int blockWidth = std::get<2>(blockRect) - std::get<0>(blockRect);
    const int blockHeight = std::get<3>(blockRect) - std::get<1>(blockRect);
    const std::tuple<int,int> size(blockWidth, blockHeight);
    return readResampledBlockChannels(blockRect, size, channelIndices, buffer, bufferSize);
}

void Scene::readResampledBlockChannels(const std::tuple<int, int, int, int>& rect,
    const std::tuple<int, int>& size, const std::vector<int>& channelIndices, void* buffer, size_t bufferSize)
{
    cv::Rect blockRect = tupleToRect(rect);
    cv::Size blockSize = tupleToSize(size);
    const int numChannels = (channelIndices.empty()?m_scene->getNumChannels():static_cast<int>(channelIndices.size()));
    const int numSlices = 1;
    const int numFrames = 1;
    const int refChannel = (channelIndices.empty()?0:channelIndices[0]);
    const int blockMemSize = getBlockSize(size, refChannel, numChannels, numSlices, numFrames);
    const auto cvType = m_scene->getChannelDataType(refChannel);

    if(blockMemSize>bufferSize)
    {
        throw std::runtime_error("Supplied memory buffer is too small");
    }
    cv::Mat raster(blockSize.height, blockSize.width, CV_MAKETYPE(static_cast<int>(cvType), numChannels), buffer);
    raster = cv::Scalar(0);
    m_scene->readResampledBlockChannels(blockRect, blockSize, channelIndices, raster);

    if(buffer!=raster.data)
    {
        throw std::runtime_error("Unexpected data reallocation");
    }
}

void Scene::read4DBlock(const std::tuple<int, int, int, int>& blockRect, const std::tuple<int, int>& zSliceRange,
    const std::tuple<int, int>& timeFrameRange, void* buffer, size_t bufferSize)
{
    const std::vector<int> channelIndices;
    return read4DBlockChannels(blockRect, channelIndices, zSliceRange, timeFrameRange, buffer, bufferSize);
}

void Scene::read4DBlockChannels(const std::tuple<int, int, int, int>& blockRect, const std::vector<int>& channelIndices,
    const std::tuple<int, int>& zSliceRange, const std::tuple<int, int>& timeFrameRange, void* buffer,
    size_t bufferSize)
{
    const int blockWidth = std::get<2>(blockRect) - std::get<0>(blockRect);
    const int blockHeight = std::get<3>(blockRect) - std::get<1>(blockRect);
    const std::tuple<int,int> blockSize(blockWidth, blockHeight);
    return readResampled4DBlockChannels(blockRect, blockSize, channelIndices, zSliceRange, timeFrameRange, buffer, bufferSize);
}

void Scene::readResampled4DBlock(const std::tuple<int, int, int, int>& blockRect, const std::tuple<int, int>& blockSize,
    const std::tuple<int, int>& zSliceRange, const std::tuple<int, int>& timeFrameRange, void* buffer,
    size_t bufferSize)
{
    const std::vector<int> channelIndices;
    return readResampled4DBlockChannels(blockRect, blockSize, channelIndices, zSliceRange, timeFrameRange, buffer, bufferSize);
}

void Scene::readResampled4DBlockChannels(const std::tuple<int, int, int, int>& rect,
    const std::tuple<int, int>& size, const std::vector<int>& channelIndices,
    const std::tuple<int, int>& zSliceRange, const std::tuple<int, int>& timeFrameRange, void* buffer,
    size_t bufferSize)
{
    cv::Rect blockRect = tupleToRect(rect);
    cv::Size blockSize = tupleToSize(size);
    cv::Range sliceRange = tupleToRange(zSliceRange);
    cv::Range frameRange = tupleToRange(timeFrameRange);

    const int numChannels = (channelIndices.empty()?m_scene->getNumChannels():static_cast<int>(channelIndices.size()));
    const int numSlices = sliceRange.size();
    const int numFrames = frameRange.size();
    const int refChannel = (channelIndices.empty()?0:channelIndices[0]);
    const int numPlanes =  numChannels*numSlices*numFrames;
    const int blockMemSize = getBlockSize(size, refChannel, numChannels, numSlices, numFrames);
    const int planeMemSize = getBlockSize(size, refChannel, numChannels, 1, 1);
    const auto cvType = m_scene->getChannelDataType(refChannel);

    if(blockMemSize>bufferSize) {
        throw std::runtime_error(
            (boost::format("Supplied memory buffer is too small. Received: %1%. Required: %2%") % bufferSize % blockMemSize).str());
    }

    cv::Mat raster(blockSize.height, blockSize.width, CV_MAKETYPE(static_cast<int>(cvType), numPlanes), buffer);
    if (numSlices==1 && numFrames==1) {
        m_scene->readResampled4DBlockChannels(blockRect, blockSize, channelIndices, sliceRange, frameRange, raster);
        if (buffer != raster.data) {
            throw std::runtime_error("Unexpected memory reallocation");
        }
    }
    else {
        cv::Mat mdRaster;
        std::vector<int> indices;
        int sliceIndex(-1), frameIndex(-1);

        if(numSlices > 1) {
            sliceIndex = 0;
            indices.push_back(0);
        }
        if( numFrames > 1) {
            frameIndex = sliceIndex + 1;
            indices.push_back(0);
        }
        int planeNum(0);
        uint8_t* planeBegin = static_cast<uint8_t*>(buffer);
        m_scene->readResampled4DBlockChannels(blockRect, blockSize, channelIndices, sliceRange, frameRange, mdRaster);
        for (int tfIndex = frameRange.start; tfIndex < frameRange.end; ++tfIndex)
        {
            if(frameIndex>=0) {
                indices[frameIndex] = tfIndex - frameRange.start;
            }
            for (int zSlieceIndex = sliceRange.start; zSlieceIndex < sliceRange.end; ++zSlieceIndex, ++planeNum, planeBegin+=planeMemSize)
            {
                if (sliceIndex >= 0) {
                    indices[sliceIndex] = zSlieceIndex - sliceRange.start;
                }
                cv::Mat sliceRaster;
                CVTools::extractSliceFromMultidimMatrix(mdRaster, indices , sliceRaster);
                if( !sliceRaster.isContinuous()) {
                    throw std::runtime_error("Unexpected non-continuous matrix");
                }
                memcpy(planeBegin, sliceRaster.data, planeMemSize);
            }
        }
    }

}

const std::list<std::string>& Scene::getAuxImageNames() const
{
    return m_scene->getAuxImageNames();
}

int Scene::getNumAuxImages() const
{
    return m_scene->getNumAuxImages();
}

std::string Scene::getRawMetadata() const
{
    return m_scene->getRawMetadata();
}

std::shared_ptr<Scene> Scene::getAuxImage(const std::string& sceneName) const
{
    std::shared_ptr<CVScene> cvScene = m_scene->getAuxImage(sceneName);
    std::shared_ptr<Scene> scene(new Scene(cvScene));
    return scene;
}


