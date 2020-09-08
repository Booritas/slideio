// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/dcm/dcmscene.hpp"
#include "slideio/drivers/dcm/dcmslide.hpp"

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
    return cv::Rect();
}

int DCMScene::getNumChannels() const
{
    return 0;
}

int DCMScene::getNumZSlices() const
{
    return 0;
}

int DCMScene::getNumTFrames() const
{
    return 0;
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
    return "";
}

Compression DCMScene::getCompression() const
{
    return Compression::Unknown;
}
