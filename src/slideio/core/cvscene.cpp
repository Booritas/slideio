// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/cvscene.hpp"
#include "slideio/imagetools/cvtools.hpp"

#include <numeric>



using namespace slideio;

std::string CVScene::getChannelName(int) const
{
    return "";
}

void CVScene::readBlock(const cv::Rect& blockRect, cv::OutputArray output)
{
    RefCounterGuard guard(this);
    const std::vector<int> channelIndices;
    readBlockChannels(blockRect, channelIndices, output);
}

void CVScene::readBlockChannels(const cv::Rect& blockRect, const std::vector<int>& channelIndices, cv::OutputArray output)
{
    RefCounterGuard guard(this);
    const cv::Rect rectScene = blockRect;
    readResampledBlockChannels(blockRect, blockRect.size(), channelIndices, output);
}

void CVScene::readResampledBlock(const cv::Rect& blockRect, const cv::Size& blockSize, cv::OutputArray output)
{
    RefCounterGuard guard(this);
    const std::vector<int> channelIndices;
    readResampledBlockChannels(blockRect, blockSize, channelIndices, output);
}

void CVScene::readResampledBlockChannels(const cv::Rect& blockRect,
    const cv::Size& blockSize, const std::vector<int>& channelIndices,
    cv::OutputArray output) {
    RefCounterGuard guard(this);
    readResampledBlockChannelsEx(blockRect, blockSize, channelIndices, 0, 0, output);
}

void CVScene::read4DBlock(const cv::Rect& blockRect, const cv::Range& zSliceRange, const cv::Range& timeFrameRange,
                          cv::OutputArray output)
{
    RefCounterGuard guard(this);
    const std::vector<int> channelIndices;
    read4DBlockChannels(blockRect, channelIndices, zSliceRange, timeFrameRange, output);
}

void CVScene::read4DBlockChannels(const cv::Rect& blockRect, const std::vector<int>& channelIndices,
    const cv::Range& zSliceRange, const cv::Range& timeFrameRange, cv::OutputArray output)
{
    RefCounterGuard guard(this);
    const cv::Rect rectScene = blockRect;
    readResampled4DBlockChannels(blockRect, blockRect.size(), channelIndices, zSliceRange, timeFrameRange, output);
}

void CVScene::readResampled4DBlock(const cv::Rect& blockRect, const cv::Size& blockSize, const cv::Range& zSliceRange,
    const cv::Range& timeFrameRange, cv::OutputArray output)
{
    RefCounterGuard guard(this);
    const std::vector<int> channelIndices;
    readResampled4DBlockChannels(blockRect, blockSize, channelIndices, zSliceRange, timeFrameRange, output);
}

void CVScene::readResampled4DBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& channelIndicesIn, const cv::Range& zSliceRange,
    const cv::Range& timeFrameRange,
    cv::OutputArray output)
{
    RefCounterGuard guard(this);
    std::vector<int> channelIndices(channelIndicesIn);
    if(channelIndices.empty()) {
        const int sceneNumChannels = getNumChannels();
        channelIndices.resize(sceneNumChannels);
        std::iota(channelIndices.begin(), channelIndices.end(), 0);
    }
    const int sliceCount = zSliceRange.end - zSliceRange.start;
    const int frameCount = timeFrameRange.end - timeFrameRange.start;
    const int channelCount = static_cast<int>(channelIndices.size());
    const int width = blockSize.width;
    const int height = blockSize.height;
    bool planeMatrix = sliceCount == 1 && frameCount == 1;
    int zDimIndex = 2;
    int tDimIndex = 3;
    if (sliceCount == 1) {
        zDimIndex = -1;
        tDimIndex = 2;
    }
    if (frameCount == 1) {
        tDimIndex = -1;
    }
    const int zLocalIndex = zDimIndex - 2;
    const int tLocalIndex = tDimIndex - 2;

    std::vector<int> dims = { height, width };
    if (zDimIndex > 0)
        dims.push_back(sliceCount);
    if (tDimIndex > 0)
        dims.push_back(frameCount);

    const slideio::DataType dt = getChannelDataType(0);
    const int cvDt = CVTools::toOpencvType(dt);
    std::vector<int> indices;

    if (planeMatrix) {
        output.create(height, width, CV_MAKE_TYPE(cvDt, channelCount));
    }
    else {
        output.create((int)dims.size(), dims.data(), CV_MAKE_TYPE(cvDt, channelCount));
    }
    cv::Mat& dataRaster = output.getMatRef();
    std::vector<cv::Range> subDims(2);
    subDims[0] = cv::Range(0, height);
    subDims[1] = cv::Range(0, width);

    if (zDimIndex > 0) {
        subDims.emplace_back(0, 0);
        indices.push_back(0);
    }
    if (tDimIndex > 0) {
        subDims.emplace_back(0, 0);
        indices.push_back(0);
    }

    for (int tfIndex = timeFrameRange.start; tfIndex < timeFrameRange.end; ++tfIndex)
    {
        if (tDimIndex > 0) {
            const int frameCounter = tfIndex - timeFrameRange.start;
            subDims[tDimIndex] = cv::Range(frameCounter, frameCounter + 1);
            indices[tLocalIndex] = frameCounter;
        }

        for (int zSlieceIndex = zSliceRange.start; zSlieceIndex < zSliceRange.end; ++zSlieceIndex)
        {
            if (zDimIndex > 0) {
                const int sliceCounter = zSlieceIndex - zSliceRange.start;
                subDims[zDimIndex] = cv::Range(sliceCounter, sliceCounter + 1);
                indices[zLocalIndex] = sliceCounter;
            }
            if (planeMatrix) {
                readResampledBlockChannelsEx(blockRect, blockSize, channelIndices, zSlieceIndex, tfIndex, dataRaster);
            }
            else {
                cv::Mat sliceRaster;
                readResampledBlockChannelsEx(blockRect, blockSize, channelIndices, zSlieceIndex, tfIndex, sliceRaster);
                CVTools::insertSliceInMultidimMatrix(dataRaster, sliceRaster, indices);
            }
        }
    }

}

std::shared_ptr<CVScene> CVScene::getAuxImage(const std::string& sceneName) const
{
    throw std::runtime_error("The scene does not have any auxiliary image");
}

void CVScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
                                           const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
    RefCounterGuard guard(this);
    if (zSliceIndex== 0 && tFrameIndex == 0)
    {
        readResampledBlockChannels(blockRect, blockSize, componentIndices, output);
    }
    else
    {
        throw std::runtime_error("4D API are not supported by this driver");
    }
}

std::vector<int> CVScene::getValidChannelIndices(const std::vector<int>& channelIndices)
{
    auto validChannelIndices(channelIndices);
    if (validChannelIndices.empty())
    {
        const  int numChannels = getNumChannels();
        validChannelIndices.resize(numChannels);
        for (int channelIndex = 0; channelIndex < numChannels; ++channelIndex)
        {
            validChannelIndices[channelIndex] = channelIndex;
        }
    }
    return validChannelIndices;
}