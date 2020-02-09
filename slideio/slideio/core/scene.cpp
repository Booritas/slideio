// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/scene.hpp"

using namespace cv::slideio;

std::string Scene::getChannelName(int) const
{
    return "";
}

void Scene::readBlock(const cv::Rect& blockRect, cv::OutputArray output)
{
    const std::vector<int> channelIndices;
    return readBlockChannels(blockRect, channelIndices, output);
}

void Scene::readBlockChannels(const cv::Rect& blockRect, const std::vector<int>& channelIndices, cv::OutputArray output)
{
    const cv::Rect rectScene = blockRect;
    return readResampledBlockChannels(blockRect, blockRect.size(), channelIndices, output);
}

void Scene::readResampledBlock(const cv::Rect& blockRect, const cv::Size& blockSize, cv::OutputArray output)
{
    const std::vector<int> channelIndices;
    return readResampledBlockChannels(blockRect, blockSize, channelIndices, output);
}

void Scene::read4DBlock(const cv::Rect& blockRect, const cv::Range& zSliceRange, const cv::Range& timeFrameRange,
    cv::OutputArray output)
{
    const std::vector<int> channelIndices;
    return read4DBlockChannels(blockRect, channelIndices, zSliceRange, timeFrameRange, output);
}

void Scene::read4DBlockChannels(const cv::Rect& blockRect, const std::vector<int>& channelIndices,
    const cv::Range& zSliceRange, const cv::Range& timeFrameRange, cv::OutputArray output)
{
    const cv::Rect rectScene = blockRect;
    return readResampled4DBlockChannels(blockRect, blockRect.size(), channelIndices, zSliceRange, timeFrameRange, output);
}

void Scene::readResampled4DBlock(const cv::Rect& blockRect, const cv::Size& blockSize, const cv::Range& zSliceRange,
    const cv::Range& timeFrameRange, cv::OutputArray output)
{
    const std::vector<int> channelIndices;
    return readResampled4DBlockChannels(blockRect, blockSize, channelIndices, zSliceRange, timeFrameRange, output);
}

void Scene::readResampled4DBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& channelIndices, const cv::Range& zSliceRange, const cv::Range& timeFrameRange,
    cv::OutputArray output)
{
    if(zSliceRange.start==0 && zSliceRange.end==0 && timeFrameRange.start==0 && timeFrameRange.end==0)
    {
        readResampledBlockChannels(blockRect, blockSize, channelIndices, output);
    }
    else
    {
        throw std::runtime_error("4D API are not supported by this driver");
    }
}
