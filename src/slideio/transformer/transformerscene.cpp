// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "transformerscene.hpp"

#include "transformationex.hpp"
#include "transformertools.hpp"
#include "slideio/core/exceptions.hpp"

using namespace slideio;

TransformerScene::TransformerScene(std::shared_ptr<CVScene> originScene,
                                   const std::list<std::shared_ptr<Transformation>>& list) :
    m_originScene(originScene), m_inflationValue(0)
{
    // Binding and channel-type accumulation are one loop, not two passes.
    //
    // Each transformation is bound against the state it will actually be
    // handed -- the channel types and colour profile the transformations
    // before it produce -- rather than against the origin scene. Two passes
    // cannot do that: the earlier version bound every element against
    // *originScene, so a composed chain such as [ColorTransformation(GRAY),
    // ColorManagement()] validated ColorManagement against three origin
    // channels, bound, and only then threw from the first tile read. The
    // accumulation also has to happen after each bind, because a bound
    // transformation may report different channel data types from its unbound
    // configuration.
    std::vector<DataType> dataTypes;
    dataTypes.reserve(originScene->getNumChannels());
    for (int channel = 0; channel < originScene->getNumChannels(); ++channel) {
        dataTypes.push_back(originScene->getChannelDataType(channel));
    }
    ColorProfile profile = originScene->getColorProfile();

    for (const auto& transformation : list) {
        TransformationEx* transformationEx = dynamic_cast<TransformationEx*>(transformation.get());
        if (!transformationEx) {
            RAISE_RUNTIME_ERROR << "TransformScene: invalid Transformation";
        }
        std::shared_ptr<TransformationEx> bound =
            transformationEx->bindToSource(*originScene, dataTypes, profile);
        if (bound) {
            transformationEx = bound.get();
        }
        m_transformations.push_back(bound ? std::static_pointer_cast<Transformation>(bound)
                                          : transformation);
        dataTypes = transformationEx->computeChannelDataTypes(dataTypes);
        profile = transformationEx->amendColorProfile(profile);
    }
    m_channelDataTypes = dataTypes;
    computeInflationValue();
}

std::string TransformerScene::getFilePath() const
{
    return m_originScene->getFilePath();
}

int TransformerScene::getSceneIndex() const {
    return m_originScene->getSceneIndex();
}

const std::string& TransformerScene::getDriverId() const {
	return m_originScene->getDriverId();
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

ColorProfile TransformerScene::getColorProfile() const
{
    ColorProfile profile = m_originScene->getColorProfile();
    for (const auto& transformation : m_transformations) {
        if (TransformationEx* transformationEx = dynamic_cast<TransformationEx*>(transformation.get())) {
            profile = transformationEx->amendColorProfile(profile);
        }
    }
    return profile;
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
        TransformationEx * transformationEx = dynamic_cast<TransformationEx*>(transformation.get());
        if(!transformationEx) {
            RAISE_RUNTIME_ERROR << "TransformScene: invalid Transformation";
        }
        transformationEx->applyTransformation(sourceBlock,targetBlock);
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

void TransformerScene::computeInflationValue()
{
    m_inflationValue = 0;
    for (const auto& transformation : m_transformations) {
        TransformationEx* transformationEx = dynamic_cast<TransformationEx*>(transformation.get());
        if (!transformationEx) {
            RAISE_RUNTIME_ERROR << "TransformScene: invalid Transformation";
        }
        m_inflationValue += transformationEx->getInflationValue();
    }
}
