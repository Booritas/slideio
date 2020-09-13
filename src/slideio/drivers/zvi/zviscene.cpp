// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/zvi/zviscene.hpp"
#include "slideio/drivers/zvi/zvislide.hpp"

using namespace slideio;

ZVIScene::ZVIScene()
{
}

std::string ZVIScene::getFilePath() const
{
    return "";
}

cv::Rect ZVIScene::getRect() const
{
    return cv::Rect();
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
