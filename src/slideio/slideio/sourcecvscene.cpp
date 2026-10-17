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
    : m_source(std::move(source)), m_metadata(std::move(metadata)), m_driverId(DRIVER_ID)
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

cv::Mat SourceCVScene::prepareBlock(const cv::Rect& rect, const char* rectLabel,
    const cv::Size& blockSize, const std::vector<int>& requested,
    int zSliceIndex, int tFrameIndex,
    std::vector<int>& channels, cv::OutputArray output) const
{
    if (blockSize.width <= 0 || blockSize.height <= 0) {
        RAISE_RUNTIME_ERROR << "SourceCVScene: block size must be positive, got "
                            << blockSize.width << "x" << blockSize.height;
    }
    if (rect.width <= 0 || rect.height <= 0) {
        RAISE_RUNTIME_ERROR << "SourceCVScene: " << rectLabel
                            << " rectangle must have positive size, got "
                            << rect.width << "x" << rect.height;
    }
    // Neither Scene nor CVScene::assemble4DBlock checks these: file drivers
    // validate them themselves, so this adapter must too, before the source
    // sees an index the scene never declared.
    if (zSliceIndex < 0 || zSliceIndex >= m_metadata.numZSlices) {
        RAISE_RUNTIME_ERROR << "SourceCVScene: z slice " << zSliceIndex
                            << " out of range [0, " << m_metadata.numZSlices << ")";
    }
    if (tFrameIndex < 0 || tFrameIndex >= m_metadata.numTFrames) {
        RAISE_RUNTIME_ERROR << "SourceCVScene: time frame " << tFrameIndex
                            << " out of range [0, " << m_metadata.numTFrames << ")";
    }
    const int numChannels = getNumChannels();
    channels = requested;
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
    return mat;
}

void SourceCVScene::readResampledBlockChannelsEx(const cv::Rect& blockRect,
    const cv::Size& blockSize, const std::vector<int>& componentIndices,
    int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
    std::vector<int> channels;
    cv::Mat mat = prepareBlock(blockRect, "block", blockSize, componentIndices,
                               zSliceIndex, tFrameIndex, channels, output);
    m_source->readBlock(Rect(blockRect), Size(blockSize.width, blockSize.height),
                        channels, zSliceIndex, tFrameIndex,
                        mat.data, mat.total() * mat.elemSize());
}

void SourceCVScene::readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
    const cv::Size& blockSize, const std::vector<int>& channelIndices,
    int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
    validateLevel(level);
    std::vector<int> channels;
    cv::Mat mat = prepareBlock(levelRect, "level", blockSize, channelIndices,
                               zSliceIndex, tFrameIndex, channels, output);

    const bool served = m_source->readLevelBlock(
        level, Rect(levelRect), Size(blockSize.width, blockSize.height),
        channels, zSliceIndex, tFrameIndex, mat.data, mat.total() * mat.elemSize());
    if (served) {
        return;
    }
    // The source has no pyramid of its own. Fall back to the base, which maps
    // the level rectangle to scene coordinates through LevelInfo::getScale()
    // and comes back through readResampledBlockChannelsEx. The base fills a
    // block that has nothing to read (rectangle wholly outside the level, or
    // clamped away) through initializeSceneBlock, which types it by channel 0
    // whatever was requested. Give it a scratch block, and keep its result
    // only if it has the type this call promised; otherwise write the
    // background ourselves (255 for 8-bit, 0 otherwise, as the base does).
    cv::Mat scratch;
    CVScene::readResampledLevelBlockChannelsEx(level, levelRect, blockSize, channels,
                                               zSliceIndex, tFrameIndex, scratch);
    if (scratch.type() == mat.type() && scratch.size() == mat.size()) {
        scratch.copyTo(mat);
    }
    else {
        mat.setTo(mat.depth() == CV_8U ? cv::Scalar::all(255) : cv::Scalar::all(0));
    }
}

bool SourceCVScene::supportsConcurrentReads() const
{
    return m_source->supportsConcurrentReads();
}

std::string SourceCVScene::getRawMetadata() const
{
    return m_source->getRawMetadata();
}

const std::list<std::string>& SourceCVScene::getAuxImageNames() const
{
    // Cached because the base returns a reference. Sources are expected to
    // report a fixed set, as every driver does.
    std::call_once(m_auxNamesOnce, [this]() {
        // Fills the base class's protected m_auxNames.
        const_cast<SourceCVScene*>(this)->m_auxNames = m_source->getAuxImageNames();
    });
    return m_auxNames;
}

int SourceCVScene::getNumAuxImages() const
{
    return static_cast<int>(getAuxImageNames().size());
}
