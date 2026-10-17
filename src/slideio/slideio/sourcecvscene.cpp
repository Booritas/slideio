// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/slideio/sourcecvscene.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include <numeric>
#include <utility>

using namespace slideio;

SourceCVScene::SourceCVScene(std::shared_ptr<ISceneSource> source, SceneMetadata metadata)
    : m_source(std::move(source)), m_metadata(std::move(metadata)), m_driverId("SOURCE")
{
}

std::string SourceCVScene::getFilePath() const      { return m_metadata.reportedFilePath; }
std::string SourceCVScene::getName() const          { return m_metadata.name; }
int SourceCVScene::getSceneIndex() const            { return 0; }
const std::string& SourceCVScene::getDriverId() const { return m_driverId; }
cv::Rect SourceCVScene::getRect() const             { return m_metadata.rect; }
int SourceCVScene::getNumChannels() const           { return static_cast<int>(m_metadata.channelDataTypes.size()); }
int SourceCVScene::getNumZSlices() const            { return m_metadata.numZSlices; }
int SourceCVScene::getNumTFrames() const            { return m_metadata.numTFrames; }
Resolution SourceCVScene::getResolution() const     { return m_metadata.resolution; }
double SourceCVScene::getZSliceResolution() const   { return m_metadata.zSliceResolution; }
double SourceCVScene::getTFrameResolution() const   { return m_metadata.tFrameResolution; }
double SourceCVScene::getMagnification() const      { return m_metadata.magnification; }
Compression SourceCVScene::getCompression() const   { return m_metadata.compression; }
ColorProfile SourceCVScene::getColorProfile() const { return m_metadata.colorProfile; }

DataType SourceCVScene::getChannelDataType(int channel) const
{
    if (channel < 0 || channel >= getNumChannels()) {
        RAISE_RUNTIME_ERROR << "SourceCVScene: channel " << channel
                            << " out of range [0, " << getNumChannels() << ")";
    }
    return m_metadata.channelDataTypes[channel];
}

std::string SourceCVScene::getChannelName(int channel) const
{
    if (channel < 0 || channel >= static_cast<int>(m_metadata.channelNames.size())) {
        return CVScene::getChannelName(channel);
    }
    return m_metadata.channelNames[channel];
}

int SourceCVScene::getChannelSignificantBits(int channel) const
{
    if (channel < 0 || channel >= static_cast<int>(m_metadata.channelSignificantBits.size())) {
        return 0;
    }
    return m_metadata.channelSignificantBits[channel];
}

int SourceCVScene::getNumZoomLevels() const
{
    return static_cast<int>(m_metadata.levels.size());
}

const LevelInfo* SourceCVScene::getZoomLevelInfo(int level) const
{
    if (level < 0 || level >= getNumZoomLevels()) {
        RAISE_RUNTIME_ERROR << "SourceCVScene: zoom level " << level
                            << " out of range [0, " << getNumZoomLevels() << ")";
    }
    return &m_metadata.levels[level];
}

void SourceCVScene::readResampledBlockChannelsEx(const cv::Rect& blockRect,
    const cv::Size& blockSize, const std::vector<int>& componentIndices,
    int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
    if (blockSize.width <= 0 || blockSize.height <= 0) {
        RAISE_RUNTIME_ERROR << "SourceCVScene: block size must be positive, got "
                            << blockSize.width << "x" << blockSize.height;
    }
    const int numChannels = getNumChannels();
    std::vector<int> channels(componentIndices);
    if (channels.empty()) {
        channels.resize(numChannels);
        std::iota(channels.begin(), channels.end(), 0);
    }
    for (int channel : channels) {
        if (channel < 0 || channel >= numChannels) {
            RAISE_RUNTIME_ERROR << "SourceCVScene: channel " << channel
                                << " out of range [0, " << numChannels << ")";
        }
    }
    // The type comes from the channels the caller asked for, deliberately not
    // from CVScene::initializeSceneBlock, which takes channel 0's type
    // whatever was requested (cvscene.cpp:330) and would size this block
    // wrongly for a mixed-type scene -- the source then writes elements of a
    // different width into it and overruns the buffer. Do not "simplify" this
    // by calling the base helper. See the design document 6.1.
    const DataType dataType = m_metadata.channelDataTypes[channels.front()];
    for (int channel : channels) {
        if (m_metadata.channelDataTypes[channel] != dataType) {
            RAISE_RUNTIME_ERROR << "SourceCVScene: all channels of one block must share a "
                                   "data type; channel " << channels.front() << " is "
                                << dataType << " and channel " << channel << " is "
                                << m_metadata.channelDataTypes[channel];
        }
    }

    const int depth = CVTools::toOpencvType(dataType);
    output.create(blockSize, CV_MAKETYPE(depth, static_cast<int>(channels.size())));
    cv::Mat mat = output.getMat();
    if (!mat.isContinuous()) {
        RAISE_RUNTIME_ERROR << "SourceCVScene: output buffer is not continuous";
    }
    m_source->readBlock(Rect(blockRect), Size(blockSize.width, blockSize.height),
                        channels, zSliceIndex, tFrameIndex,
                        mat.data, mat.total() * mat.elemSize());
}
