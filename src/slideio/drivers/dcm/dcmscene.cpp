// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/dcm/dcmscene.hpp"
#include "slideio/drivers/dcm/dcmslide.hpp"
#include <set>
#include <boost/format.hpp>


using namespace slideio;

DCMScene::DCMScene()
{
}

std::string DCMScene::getFilePath() const
{
    return "";
}

cv::Rect DCMScene::getRect() const
{
    return m_rect;
}

int DCMScene::getNumChannels() const
{
    return m_numChannels;
}

int DCMScene::getNumZSlices() const
{
    return m_numSlices;
}

int DCMScene::getNumTFrames() const
{
    return m_numFrames;
}

double DCMScene::getZSliceResolution() const
{
    return 0;
}

double DCMScene::getTFrameResolution() const
{
    return 0;
}

slideio::DataType DCMScene::getChannelDataType(int channel) const
{
    return slideio::DataType::DT_Unknown;
}

std::string DCMScene::getChannelName(int channel) const
{
    return "";
}

Resolution DCMScene::getResolution() const
{
    return Resolution();
}

double DCMScene::getMagnification() const
{
    return 0;
}

void DCMScene::readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& componentIndices, cv::OutputArray output)
{
}

void DCMScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
}

void DCMScene::readResampled4DBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& channelIndices, const cv::Range& zSliceRange, const cv::Range& timeFrameRange,
    cv::OutputArray output)
{
}

std::string DCMScene::getName() const
{
    return m_name;
}

Compression DCMScene::getCompression() const
{
    return Compression::Unknown;
}

void DCMScene::addFile(std::shared_ptr<DCMFile>& file)
{
    m_files.push_back(file);
}

void DCMScene::init()
{
    if (m_files.empty()) {
        std::string message = "DCMImageDriver: attempt to create an empty scene";
        throw std::runtime_error(message);
    }
    std::set<std::tuple<int, int>> sizes;
    std::set<std::string> series;
    std::set<int> channelCounts;
    int slices = 0;
    for(auto&& file: m_files) {
        sizes.insert({ file->getWidth(), file->getHeight() });
        slices += file->getNumSlices();
        series.insert(file->getSeriesUID());
        channelCounts.insert(file->getNumChannels());
    }
    if(sizes.size()!=1) {
        std::string message = 
            (boost::format("DCMImageDriver: Unexpected number of different sizes in a scene. Expected: 1. Found: %1%")
                % sizes.size()).str();
        throw std::runtime_error(message);
    }
    if (series.size() != 1) {
        std::string message =
            (boost::format("DCMImageDriver: Unexpected number of different series UID in a scene. Expected: 1. Found: %1%")
                % series.size()).str();
        throw std::runtime_error(message);
    }
    if (channelCounts.size() != 1) {
        std::string message = "DCMImageDriver: All frames shall have the same number of channels";
        throw std::runtime_error(message);
    }
    auto size = *sizes.begin();
    m_rect = { 0, 0, std::get<0>(size), std::get<1>(size) };
    m_numSlices = slices;
    m_name = *(series.begin());
    m_numChannels = *(channelCounts.begin());
    if(m_files.size()>1) {
        std::sort(m_files.begin(), m_files.end(),
            [](std::shared_ptr<DCMFile>& left, std::shared_ptr<DCMFile>& right)
            {
                return left->getInstanceNumber() < right->getInstanceNumber();
            });
    }
}
