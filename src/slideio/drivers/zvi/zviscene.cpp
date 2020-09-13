// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/zvi/zviscene.hpp"
#include "slideio/drivers/zvi/zvislide.hpp"
#include <boost/filesystem.hpp>
#include <pole/polepp.hpp>
#include <boost/format.hpp>

#include "ZVIUtils.hpp"

using namespace slideio;

ZVIScene::ZVIScene(const std::string& filePath) : m_filePath(filePath), m_Doc(filePath)
{
    init();
}

std::string ZVIScene::getFilePath() const
{
    return m_filePath;
}

cv::Rect ZVIScene::getRect() const
{
    return cv::Rect(0,0, m_Width, m_Height);
}

int ZVIScene::getNumChannels() const
{
    return 0;
}

int ZVIScene::getNumZSlices() const
{
    return 0;
}

int ZVIScene::getNumTFrames() const
{
    return 0;
}

double ZVIScene::getZSliceResolution() const
{
    return 0;
}

double ZVIScene::getTFrameResolution() const
{
    return 0;
}

slideio::DataType ZVIScene::getChannelDataType(int channel) const
{
    return slideio::DataType::DT_Unknown;
}

std::string ZVIScene::getChannelName(int channel) const
{
    return "";
}

Resolution ZVIScene::getResolution() const
{
    return Resolution();
}

double ZVIScene::getMagnification() const
{
    return 0;
}

void ZVIScene::readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& componentIndices, cv::OutputArray output)
{
}

void ZVIScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
}

void ZVIScene::readResampled4DBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& channelIndices, const cv::Range& zSliceRange, const cv::Range& timeFrameRange,
    cv::OutputArray output)
{
}

std::string ZVIScene::getName() const
{
    return "";
}

Compression ZVIScene::getCompression() const
{
    return Compression::Unknown;
}

void ZVIScene::init()
{
    namespace fs = boost::filesystem;
    if (!fs::exists(m_filePath)) {
        throw std::runtime_error(std::string("ZVIImageDriver: File does not exist:") + m_filePath);
    }
    if (!m_Doc.good())
    {
        throw std::runtime_error(
            (boost::format("Cannot open compound file %1%") % m_filePath).str());
    }
    auto imageStorageItr = m_Doc.find_storage("Image");
    if (imageStorageItr == m_Doc.end())
    {
        throw std::runtime_error("ZVIImageDriver: Invalid storage. Missing 'Image' directory");
    }
    auto imageStorage = *imageStorageItr;
    auto contentsStreamItr = imageStorage.find_stream("Image/Contents");
    if (contentsStreamItr == imageStorage.end())
    {
        throw std::runtime_error("ZVIImageDriver: Invalid storage. Missing 'Image/Contents' stream");
    }
    ZVIUtils::skipItems(contentsStreamItr->stream(), 4);
    m_Width = ZVIUtils::readIntItem(contentsStreamItr->stream());
    m_Height = ZVIUtils::readIntItem(contentsStreamItr->stream());
    int depth = ZVIUtils::readIntItem(contentsStreamItr->stream());
    m_PixelFormat = ZVIUtils::readIntItem(contentsStreamItr->stream());
    m_RawCount = ZVIUtils::readIntItem(contentsStreamItr->stream());
}
