// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/dcm/dcmscene.hpp"
#include "slideio/drivers/dcm/dcmslide.hpp"
#include <set>
#include <boost/format.hpp>

#include <opencv2/imgproc.hpp>

#include "slideio/base.hpp"
#include "slideio/imagetools/tools.hpp"


using namespace slideio;

DCMScene::DCMScene() {
}

std::string DCMScene::getFilePath() const {
    return m_filePath;
}

cv::Rect DCMScene::getRect() const {
    return m_rect;
}

int DCMScene::getNumChannels() const {
    return m_numChannels;
}

int DCMScene::getNumZSlices() const {
    return m_numSlices;
}

int DCMScene::getNumTFrames() const {
    return m_numFrames;
}

double DCMScene::getZSliceResolution() const {
    return 0;
}

double DCMScene::getTFrameResolution() const {
    return 0;
}

slideio::DataType DCMScene::getChannelDataType(int channel) const {
    return slideio::DataType::DT_Unknown;
}

std::string DCMScene::getChannelName(int channel) const {
    return "";
}

Resolution DCMScene::getResolution() const {
    return Resolution();
}

double DCMScene::getMagnification() const {
    return 0;
}

std::string DCMScene::getName() const {
    return m_name;
}

Compression DCMScene::getCompression() const {
    return Compression::Unknown;
}

void DCMScene::addFile(std::shared_ptr<DCMFile>& file) {
    m_files.push_back(file);
}

void DCMScene::init() {
    SLIDEIO_LOG(trace) << "DCMScene::init-begin";
    if (m_files.empty()) {
        SLIDEIO_LOG(error) <<
 "DCMScene::init attempt to create an empty scene.";
        std::string message =
            "DCMImageDriver: attempt to create an empty scene";
        throw std::runtime_error(message);
    }
    m_filePath = (*m_files.begin())->getFilePath();
    std::set<std::tuple<int, int>> sizes;
    std::set<std::string> series;
    std::set<int> channelCounts;
    int slices = 0;
    for (auto&& file : m_files) {
        sizes.insert({file->getWidth(), file->getHeight()});
        slices += file->getNumSlices();
        series.insert(file->getSeriesUID());
        channelCounts.insert(file->getNumChannels());
    }
    if (sizes.size() != 1) {
        std::string message =
        (boost::format(
                "DCMImageDriver: Unexpected number of different sizes in a scene. Expected: 1. Found: %1%")
            % sizes.size()).str();
        SLIDEIO_LOG(error) << message;
        throw std::runtime_error(message);
    }
    if (series.size() != 1) {
        std::string message =
        (boost::format(
                "DCMImageDriver: Unexpected number of different series UID in a scene. Expected: 1. Found: %1%")
            % series.size()).str();
        SLIDEIO_LOG(error) << message;
        throw std::runtime_error(message);
    }
    if (channelCounts.size() != 1) {
        std::string message =
            "DCMImageDriver: All frames shall have the same number of channels";
        SLIDEIO_LOG(error) << message;
        throw std::runtime_error(message);
    }
    auto size = *sizes.begin();
    m_rect = {0, 0, std::get<0>(size), std::get<1>(size)};
    m_numSlices = slices;
    m_name = *(series.begin());
    const std::string seriesDescription = (*(m_files.begin()))->
        getSeriesDescription();
    if (!seriesDescription.empty()) {
        m_name = seriesDescription;
    }
    m_numChannels = *(channelCounts.begin());
    if (m_files.size() > 1) {
        std::sort(m_files.begin(), m_files.end(),
                  [](std::shared_ptr<DCMFile>& left,
                     std::shared_ptr<DCMFile>& right)
                  {
                      return left->getInstanceNumber() < right->
                          getInstanceNumber();
                  });
    }
    SLIDEIO_LOG(trace) << " DCMScene::init-end";
}

void DCMScene::initializeCounter() {
    if (m_files.size() == 1) {
        SLIDEIO_LOG(trace) << "DCMImageDriver: create raster scene cache.";
        auto file = m_files.front();
        file->readPixelValues(m_frameRasters);
    }
}

void DCMScene::cleanCounter() {
    if (!m_frameRasters.empty()) {
        SLIDEIO_LOG(trace) << "DCMImageDriver: clean raster scene cache.";
        m_frameRasters.clear();
    }
}

void DCMScene::extractSliceRaster(const std::vector<cv::Mat>& frames,
                                  const cv::Rect& blockRect,
                                  const cv::Size& blockSize,
                                  const std::vector<int>& componentIndices,
                                  int zSliceIndex, cv::OutputArray output) {
    if (zSliceIndex >= frames.size()) {
        RAISE_RUNTIME_ERROR << "DCMImageDriver: frame index: " <<
            zSliceIndex << " exceeds number of frames: " << frames.size();
    }
    cv::Mat block = frames[zSliceIndex](blockRect);
    cv::Mat resizedBlock;
    cv::resize(block, resizedBlock, blockSize);
    if (componentIndices.empty() || (componentIndices.size() == getNumChannels()
        && getNumChannels() == 1)) {
        resizedBlock.copyTo(output);
    }
    else {
        std::vector<int> channelIndices = Tools::completeChannelList(
            componentIndices, getNumChannels());
        std::vector<cv::Mat> channelRasters(channelIndices.size());
        for (int index = 0; index < channelIndices.size(); ++index) {
            int channelIndex = channelIndices[index];
            cv::extractChannel(resizedBlock, channelRasters[index],
                               channelIndex);
        }
        cv::merge(channelRasters, output);
    }
}

void DCMScene::readResampledBlockChannelsEx(const cv::Rect& blockRect,
                                            const cv::Size& blockSize,
                                            const std::vector<int>&
                                            componentIndices,
                                            int zSliceIndex,
                                            int tFrameIndex,
                                            cv::OutputArray output) {
    SLIDEIO_LOG(trace) << "DCMImageDriver: Resample block:" << std::endl
        << "block: " << blockRect.x << "," << blockRect.y << ","
        << blockRect.width << "," << blockRect.height << std::endl
        << "size: " << blockSize.width << "," << blockSize.height << std::endl
        << "channels:" << componentIndices.size() << std::endl
        << "slice: " << zSliceIndex << std::endl
        << "frame: " << tFrameIndex;
    const std::vector<cv::Mat>& frames = m_frameRasters;

    if (m_frameRasters.empty()) {
        RAISE_RUNTIME_ERROR <<
            "Multiple file series reader is not implemented!";
    }
    else {
        extractSliceRaster(frames, blockRect, blockSize, componentIndices,
                           zSliceIndex,  output);
    }
}
