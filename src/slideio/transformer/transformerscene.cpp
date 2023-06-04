// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "transformerscene.hpp"

#include "transformation.hpp"
#include "transformertools.hpp"

using namespace slideio;

TransformerScene::TransformerScene(std::shared_ptr<CVScene> originScene,
                                   const std::list<std::shared_ptr<Transformation>>& list) :
    m_originScene(originScene), m_transformations(list), m_inflationValue(0)
{
    initChannels();
    computeInflationValue();
}

std::string TransformerScene::getFilePath() const
{
    return m_originScene->getFilePath();
}

std::string TransformerScene::getName() const
{
    return m_originScene->getName();
}

cv::Rect TransformerScene::getRect() const
{
    return m_originScene->getRect();
}

int TransformerScene::getNumChannels() const
{
    return (int)m_channelDataTypes.size();
}

slideio::DataType TransformerScene::getChannelDataType(int channel) const
{
    return m_channelDataTypes[channel];
}

Resolution TransformerScene::getResolution() const
{
    return m_originScene->getResolution();
}

double TransformerScene::getMagnification() const
{
    return m_originScene->getMagnification();
}

Compression TransformerScene::getCompression() const
{
    return m_originScene->getCompression();
}

int TransformerScene::getNumZSlices() const
{
    return m_originScene->getNumZSlices();
}

int TransformerScene::getNumTFrames() const
{
    return m_originScene->getNumTFrames();
}

std::string TransformerScene::getChannelName(int channel) const
{
    return m_originScene->getChannelName(channel);
}

double TransformerScene::getZSliceResolution() const
{
    return m_originScene->getZSliceResolution();
}

double TransformerScene::getTFrameResolution() const
{
    return m_originScene->getTFrameResolution();
}

std::string TransformerScene::getRawMetadata() const
{
    return m_originScene->getRawMetadata();
}

void TransformerScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
    cv::Rect extendedBlockRect;
    cv::Size extendedBlockSize;
    cv::Point blockPosition;
    const cv::Rect sceneRect = getRect();
    const cv::Size sceneSize(sceneRect.size());
    TransformerTools::computeInflatedRectParams(sceneSize, blockRect, m_inflationValue, blockSize, extendedBlockRect,
        extendedBlockSize, blockPosition);
    cv::Mat sourceBlock;
    getOriginScene()->readResampledBlockChannelsEx(extendedBlockRect, extendedBlockSize, {}, zSliceIndex,
        tFrameIndex, sourceBlock);

    for (const auto& transformation : m_transformations) {
        cv::Mat targetBlock;
        transformation->applyTransformation(sourceBlock,targetBlock);
        targetBlock.copyTo(sourceBlock);
    }

    cv::Rect rectInInflatedRect = cv::Rect(blockPosition.x, blockPosition.y, blockSize.width, blockSize.height);
    cv::Mat block = sourceBlock(rectInInflatedRect);
    if(componentIndices.empty()) {
        block.copyTo(output);
    }
    else {
        std::vector<cv::Mat> channels;
        cv::split(block, channels);
        std::vector<cv::Mat> selectedChannels;
        selectedChannels.reserve(componentIndices.size());
        for (const auto index : componentIndices) {
            selectedChannels.push_back(channels[index]);
        }
        cv::merge(selectedChannels, output);
    }
}

void TransformerScene::initChannels()
{
    const int numChannels = m_originScene->getNumChannels();
    std::vector<DataType> dataTypes;
    for (int ch = 0; ch < numChannels; ++ch) {
        dataTypes.push_back(m_originScene->getChannelDataType(ch));
    }
    for (const auto& transformation : m_transformations) {
        std::vector<DataType> newDataTypes = transformation->computeChannelDataTypes(dataTypes);
        dataTypes = newDataTypes;
    }
    m_channelDataTypes = dataTypes;
}

void TransformerScene::computeInflationValue()
{
    m_inflationValue = 0;
    for (const auto& transformation : m_transformations) {
        m_inflationValue += transformation->getInflationValue();
    }
}
