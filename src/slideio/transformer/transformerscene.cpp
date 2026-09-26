// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "transformerscene.hpp"

#include "transformationex.hpp"
#include "transformertools.hpp"
#include "slideio/core/exceptions.hpp"
#include <algorithm>
#include <cmath>

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
        // computeColorProfile, not amendColorProfile: this accumulator says what
        // the *next* transformation will be handed, and after a colour
        // conversion that is the target space, not the source the scene still
        // reports. getColorProfile() below keeps using amendColorProfile, which
        // is the provenance question and has the opposite answer.
        profile = transformationEx->computeColorProfile(profile);
    }
    m_channelDataTypes = dataTypes;
    computeInflationValue();

    // The origin's pyramid, copied verbatim. A transformation changes what a
    // pixel holds -- its channel count and type -- but never where it is: each
    // one is handed a block and returns a block of the same size, and getRect()
    // forwards the origin's. So every level keeps its geometry, scale and
    // magnification, and nothing here has to be recomputed.
    m_levels.reserve(originScene->getNumZoomLevels());
    for (int level = 0; level < originScene->getNumZoomLevels(); ++level) {
        m_levels.push_back(*originScene->getZoomLevelInfo(level));
    }
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

// A transformation changes pixel values, not when or how the origin was
// acquired, so everything describing the acquisition is the origin's answer.

int TransformerScene::getChannelSignificantBits(int channelIndex) const
{
    return m_originScene->getChannelSignificantBits(channelIndex);
}

bool TransformerScene::hasPlaneTimestamps() const
{
    return m_originScene->hasPlaneTimestamps();
}

double TransformerScene::getPlaneTimestamp(int tFrame, int channel, int zSlice) const
{
    return m_originScene->getPlaneTimestamp(tFrame, channel, zSlice);
}

int64_t TransformerScene::getAcquisitionTime() const
{
    return m_originScene->getAcquisitionTime();
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

    applyChain(sourceBlock, blockPosition, blockSize, componentIndices, output);
}

void TransformerScene::readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
    const cv::Size& blockSize, const std::vector<int>& componentIndices,
    int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
    // Deliberately not the base class implementation, which would convert the
    // level rectangle to scene coordinates and read through
    // readResampledBlockChannelsEx -- leaving the origin to pick a level for
    // itself from the resulting scale. That would usually land on the level the
    // caller named, but "usually" is not what the level api promises. The
    // origin is asked for the level it was asked for.
    validateLevel(level);
    const LevelInfo* levelInfo = getZoomLevelInfo(level);
    const cv::Size levelSize = levelInfo->getSize();
    const cv::Rect levelBounds(0, 0, levelSize.width, levelSize.height);
    const cv::Rect validRect = levelRect & levelBounds;

    // The whole block is background first, and only the part the level actually
    // covers is overwritten -- the contract the base class defines and an
    // override has to keep. The guards come before any scaling arithmetic
    // because computeInflatedRectParams divides by the requested rectangle's
    // width and height, so a degenerate rectangle would reach a division by
    // zero on the way to producing an empty block for the chain to choke on.
    initializeSceneBlock(blockSize, componentIndices, output);
    if (validRect.empty() || blockSize.width <= 0 || blockSize.height <= 0
        || levelRect.width <= 0 || levelRect.height <= 0) {
        return;
    }

    // Where the surviving part of the rectangle lands in the output. Derived
    // from the offsets rather than from the width so that a rectangle clipped
    // on both sides keeps both -- the same arithmetic as the base class, and
    // deliberately identical so the two cannot disagree about placement.
    const double scaleX = static_cast<double>(blockSize.width) / static_cast<double>(levelRect.width);
    const double scaleY = static_cast<double>(blockSize.height) / static_cast<double>(levelRect.height);
    cv::Rect target;
    target.x = static_cast<int>(std::floor((validRect.x - levelRect.x) * scaleX));
    target.y = static_cast<int>(std::floor((validRect.y - levelRect.y) * scaleY));
    target.width = std::min(static_cast<int>(std::ceil(validRect.width * scaleX)),
                            blockSize.width - target.x);
    target.height = std::min(static_cast<int>(std::ceil(validRect.height * scaleY)),
                             blockSize.height - target.y);
    if (target.width <= 0 || target.height <= 0) {
        return;
    }

    // The *clipped* rectangle is what gets inflated, at the size it occupies in
    // the output. Inflating the requested rectangle instead leaves the crop in
    // applyChain describing a region the origin was never asked for, which for
    // anything reaching outside the level is an invalid ROI.
    cv::Rect extendedLevelRect;
    cv::Size extendedBlockSize;
    cv::Point blockPosition;
    TransformerTools::computeInflatedRectParams(levelSize, validRect, m_inflationValue, target.size(),
        extendedLevelRect, extendedBlockSize, blockPosition);

    cv::Mat sourceBlock;
    getOriginScene()->readResampledLevelBlockChannelsEx(level, extendedLevelRect, extendedBlockSize,
        {}, zSliceIndex, tFrameIndex, sourceBlock);

    cv::Mat part;
    applyChain(sourceBlock, blockPosition, target.size(), componentIndices, part);
    cv::Mat block = output.getMat();
    part.copyTo(block(target));
}

void TransformerScene::applyChain(cv::Mat& sourceBlock, const cv::Point& blockPosition,
    const cv::Size& blockSize, const std::vector<int>& componentIndices, cv::OutputArray output)
{
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
    // The caller's geometry should already place this inside the block, but the
    // inflation arithmetic rounds, and a crop one pixel over the edge surfaces
    // as a bare OpenCV ROI assertion that says nothing about which read failed.
    // Nudge a rounding overshoot back, and raise something legible if the block
    // is genuinely too small -- that would be a logic error above, not input.
    rectInInflatedRect.x = std::max(0, std::min(rectInInflatedRect.x,
                                               sourceBlock.cols - rectInInflatedRect.width));
    rectInInflatedRect.y = std::max(0, std::min(rectInInflatedRect.y,
                                               sourceBlock.rows - rectInInflatedRect.height));
    if (rectInInflatedRect.width > sourceBlock.cols || rectInInflatedRect.height > sourceBlock.rows) {
        RAISE_RUNTIME_ERROR << "TransformerScene: transformed block is "
            << sourceBlock.cols << "x" << sourceBlock.rows << ", too small for the requested "
            << rectInInflatedRect.width << "x" << rectInInflatedRect.height;
    }
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
