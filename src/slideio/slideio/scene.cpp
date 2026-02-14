// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "slideio/slideio/scene.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/base/log.hpp"
#include "slideio/base/exceptions.hpp"

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
    SLIDEIO_LOG(INFO) << "Scene constructor"; 
}

std::string Scene::getFilePath() const
{
    SLIDEIO_LOG(INFO) << "Scene::getFilePath "; 
    return m_scene->getFilePath();
}

std::string Scene::getName() const
{
    SLIDEIO_LOG(INFO) << "Scene::getName "; 
    return m_scene->getName();
}

std::tuple<int, int, int, int> Scene::getRect() const
{
    SLIDEIO_LOG(INFO) << "Scene::getRect "; 
    cv::Rect rect = m_scene->getRect();
    std::tuple<int,int,int,int> wrapper(rect.x, rect.y, rect.width, rect.height);
    return wrapper;
}

int Scene::getNumChannels() const
{
    SLIDEIO_LOG(INFO) << "Scene::getNumChannels "; 
    return m_scene->getNumChannels();
}

int Scene::getNumZSlices()
{
    SLIDEIO_LOG(INFO) << "Scene::getNumZSlices "; 
    return m_scene->getNumZSlices();
}

int Scene::getNumTFrames()
{
    SLIDEIO_LOG(INFO) << "Scene::getNumTFrames "; 
    return m_scene->getNumTFrames();
}

Compression Scene::getCompression() const
{
    SLIDEIO_LOG(INFO) << "Scene::getCompression "; 
    return m_scene->getCompression();
}

slideio::DataType Scene::getChannelDataType(int channel) const
{
    SLIDEIO_LOG(INFO) << "Scene::getChannelDataType "; 
    return m_scene->getChannelDataType(channel);
}

std::string Scene::getChannelName(int channel) const
{
    SLIDEIO_LOG(INFO) << "Scene::getChannelName "; 
    return m_scene->getChannelName(channel);
}

std::tuple<double, double> Scene::getResolution() const
{
    SLIDEIO_LOG(INFO) << "Scene::getResolution "; 
    slideio::Resolution res = m_scene->getResolution();
    std::tuple<double,double> wrapper(res.x, res.y);
    return wrapper;
}

double Scene::getZSliceResolution() const
{
    SLIDEIO_LOG(INFO) << "Scene::getZSliceResolution "; 
    return m_scene->getZSliceResolution();
}

double Scene::getTFrameResolution() const
{
    SLIDEIO_LOG(INFO) << "Scene::getTFrameResolution "; 
    return m_scene->getTFrameResolution();
}

double Scene::getMagnification() const
{
    SLIDEIO_LOG(INFO) << "Scene::getMagnification "; 
    return m_scene->getMagnification();
}

int Scene::getBlockSize(const std::tuple<int, int>& blockSize, int refChannel, int numChannels, int numSlices, int numFrames) const
{
    SLIDEIO_LOG(INFO) << "Scene::getBlockSize ";// << blockSize << "," << refChannel << "," << numChannels << "," << numSlices << "," << numFrames;
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
    SLIDEIO_LOG(INFO) << "Scene::readBlock ";// << rect;
    const std::vector<int> channelIndices;
    return readBlockChannels(rect, channelIndices, buffer, bufferSize);
}

void Scene::readBlockChannels(const std::tuple<int, int, int, int>& blockRect, const std::vector<int>& channelIndices,
    void* buffer, size_t bufferSize)
{
    SLIDEIO_LOG(INFO) << "Scene::readBlockChannels ";// << blockRect << "," << channelIndices;
    const int blockWidth = std::get<2>(blockRect);
    const int blockHeight = std::get<3>(blockRect);
    const std::tuple<int,int> size(blockWidth, blockHeight);
    return readResampledBlockChannels(blockRect, size, channelIndices, buffer, bufferSize);
}

void Scene::readResampledBlock(const std::tuple<int, int, int, int>& blockRect, const std::tuple<int, int>& blockSize,
    void* buffer, size_t bufferSize)
{
    SLIDEIO_LOG(INFO) << "Scene::readResampledBlock ";// << blockRect << "," << blockSize;
    const std::vector<int> channelIndices;
    return readResampledBlockChannels(blockRect, blockSize, channelIndices, buffer, bufferSize);
}

void Scene::readResampledBlockChannels(const std::tuple<int, int, int, int>& rect,
                                       const std::tuple<int, int>& size, const std::vector<int>& channelIndices, void* buffer, size_t bufferSize)
{
    SLIDEIO_LOG(INFO) << "Scene::readResampledBlockChannels ";// << rect << "," << size << "," << channelIndices;
    cv::Rect blockRect = tupleToRect(rect);
    cv::Size blockSize = tupleToSize(size);
    const int numChannels = (channelIndices.empty()?m_scene->getNumChannels():static_cast<int>(channelIndices.size()));
    const int numSlices = 1;
    const int numFrames = 1;
    const int refChannel = (channelIndices.empty()?0:channelIndices[0]);
    const int blockMemSize = getBlockSize(size, refChannel, numChannels, numSlices, numFrames);
    const auto dt = m_scene->getChannelDataType(refChannel);
    int cvType = CVTools::cvTypeFromDataType(dt);

    if(blockMemSize>bufferSize)
    {
        RAISE_RUNTIME_ERROR << "Supplied memory buffer is too small";
    }
    cv::Mat raster(blockSize.height, blockSize.width, CV_MAKETYPE(cvType, numChannels), buffer);
    raster = cv::Scalar(0);
    m_scene->readResampledBlockChannels(blockRect, blockSize, channelIndices, raster);

    if(buffer!=raster.data)
    {
        RAISE_RUNTIME_ERROR << "Unexpected data reallocation by reading of file " << getFilePath();
    }
}

void Scene::read4DBlock(const std::tuple<int, int, int, int>& blockRect, const std::tuple<int, int>& zSliceRange,
    const std::tuple<int, int>& timeFrameRange, void* buffer, size_t bufferSize)
{
    SLIDEIO_LOG(INFO) << "Scene::read4DBlock ";// << blockRect << "," << zSliceRange << "," << timeFrameRange;
    const std::vector<int> channelIndices;
    return read4DBlockChannels(blockRect, channelIndices, zSliceRange, timeFrameRange, buffer, bufferSize);
}

void Scene::read4DBlockChannels(const std::tuple<int, int, int, int>& blockRect, const std::vector<int>& channelIndices,
    const std::tuple<int, int>& zSliceRange, const std::tuple<int, int>& timeFrameRange, void* buffer,
    size_t bufferSize)
{
    SLIDEIO_LOG(INFO) << "Scene::read4DBlock ";// << blockRect << "," << channelIndices << "," << zSliceRange << "," << timeFrameRange;
    const int blockWidth = std::get<2>(blockRect);
    const int blockHeight = std::get<3>(blockRect);
    const std::tuple<int,int> blockSize(blockWidth, blockHeight);
    return readResampled4DBlockChannels(blockRect, blockSize, channelIndices, zSliceRange, timeFrameRange, buffer, bufferSize);
}

void Scene::readResampled4DBlock(const std::tuple<int, int, int, int>& blockRect, const std::tuple<int, int>& blockSize,
    const std::tuple<int, int>& zSliceRange, const std::tuple<int, int>& timeFrameRange, void* buffer,
    size_t bufferSize)
{
    SLIDEIO_LOG(INFO) << "Scene::readResampled4DBlock ";// << blockRect << "," << zSliceRange << "," << timeFrameRange;
    const std::vector<int> channelIndices;
    return readResampled4DBlockChannels(blockRect, blockSize, channelIndices, zSliceRange, timeFrameRange, buffer, bufferSize);
}

void Scene::readResampled4DBlockChannels(const std::tuple<int, int, int, int>& rect,
    const std::tuple<int, int>& size, const std::vector<int>& channelIndices,
    const std::tuple<int, int>& zSliceRange, const std::tuple<int, int>& timeFrameRange, void* buffer,
    size_t bufferSize)
{
    SLIDEIO_LOG(INFO) << "Scene::readResampled4DBlockChannels ";// << rect << "," << size << "," << channelIndices << "," << zSliceRange << "," << timeFrameRange;
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
        RAISE_RUNTIME_ERROR << "Supplied memory buffer is too small. Received: " << bufferSize << ". Required: " << blockMemSize;
    }

    cv::Mat raster(blockSize.height, blockSize.width, CV_MAKETYPE(static_cast<int>(cvType), numPlanes), buffer);
    if (numSlices==1 && numFrames==1) {
        m_scene->readResampled4DBlockChannels(blockRect, blockSize, channelIndices, sliceRange, frameRange, raster);
        if (buffer != raster.data) {
            RAISE_RUNTIME_ERROR << "Unexpected memory reallocation";
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
                    RAISE_RUNTIME_ERROR << "Unexpected non-continuous matrix";
                }
                memcpy(planeBegin, sliceRaster.data, planeMemSize);
            }
        }
    }

}

const std::list<std::string>& Scene::getAuxImageNames() const
{
    SLIDEIO_LOG(INFO) << "Scene::getAuxImageNames "; 
    return m_scene->getAuxImageNames();
}

int Scene::getNumAuxImages() const
{
    SLIDEIO_LOG(INFO) << "Scene::getNumAuxImages "; 
    return m_scene->getNumAuxImages();
}

std::string Scene::getRawMetadata() const
{
    SLIDEIO_LOG(INFO) << "Scene::getRawMetadata "; 
    return m_scene->getRawMetadata();
}

MetadataFormat Scene::getMetadataFormat() const {
	SLIDEIO_LOG(INFO) << "Scene::getMetadataFormat ";
	return m_scene->getMetadataFormat();
}

std::shared_ptr<Scene> Scene::getAuxImage(const std::string& sceneName) const
{
    SLIDEIO_LOG(INFO) << "Scene::getAuxImage " << sceneName; 
    std::shared_ptr<CVScene> cvScene = m_scene->getAuxImage(sceneName);
    std::shared_ptr<Scene> scene(new Scene(cvScene));
    return scene;
}

int Scene::getNumZoomLevels() const {
   return m_scene->getNumZoomLevels();
}

const LevelInfo* Scene::getLevelInfo(int level) const {
	return m_scene->getZoomLevelInfo(level);
}

std::string Scene::toString() const {
    return m_scene->toString();
}

int Scene::getNumChannelAttributes() const {
	return m_scene->getNumChannelAttributes();
}

int Scene::getChannelAttributeIndex(const std::string& attributeName) const {
	return m_scene->getChannelAttributeIndex(attributeName);
}

std::string Scene::getChannelAttributeName(int attributeIndex) const {
	return m_scene->getChannelAttributeName(attributeIndex);
}

std::string Scene::getChannelAttributeValue(int channelIndex, const std::string& attributeName) const {
	return m_scene->getChannelAttributeValue(channelIndex, attributeName);
}


