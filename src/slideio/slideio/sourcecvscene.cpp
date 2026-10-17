// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/slideio/sourcecvscene.hpp"
#include "slideio/slideio/sceneinternal.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include <numeric>
#include <utility>

using namespace slideio;

SourceCVScene::SourceCVScene(std::shared_ptr<ISceneSource> source, SceneMetadata metadata)
    : m_source(std::move(source)), m_metadata(std::move(metadata)), m_driverId(DRIVER_ID)
{
    // Into the base members, so getRawMetadata(), getMetadataFormat() and the
    // structured tree getMetadata() builds from them all report the same thing.
    m_rawMetadata = m_source->getRawMetadata();
    m_metadataFormat = m_source->getMetadataFormat();
}

bool SourceCVScene::canReopenByPath() const { return false; }

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

std::vector<int> SourceCVScene::validateRequest(const std::vector<int>& requested,
    int zSliceIndex, int tFrameIndex) const
{
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
    std::vector<int> channels = requested;
    if (channels.empty()) {
        channels.resize(numChannels);
        std::iota(channels.begin(), channels.end(), 0);
    }
    if (channels.empty()) {
        // Only reachable for a scene constructed directly with no channels:
        // makeScene rejects such metadata.
        RAISE_RUNTIME_ERROR << "SourceCVScene: the scene declares no channels";
    }
    for (int channel : channels) {
        if (channel < 0 || channel >= numChannels) {
            RAISE_RUNTIME_ERROR << "SourceCVScene: channel " << channel
                                << " out of range [0, " << numChannels << ")";
        }
    }
    // A cv::Mat carries one depth, so a block cannot mix channel types: the
    // source would write elements of a different width than the block holds.
    // A scene whose channels differ is read one type per call.
    const DataType dataType = m_metadata.channelDataTypes[channels.front()];
    for (int channel : channels) {
        if (m_metadata.channelDataTypes[channel] != dataType) {
            RAISE_RUNTIME_ERROR << "SourceCVScene: all channels of one block must share a "
                                   "data type; channel " << channels.front() << " is "
                                << dataType << " and channel " << channel << " is "
                                << m_metadata.channelDataTypes[channel];
        }
    }
    return channels;
}

cv::Mat SourceCVScene::allocateBlock(const cv::Size& blockSize,
    const std::vector<int>& channels, cv::OutputArray output) const
{
    // Typed by CVScene::blockDataType, the rule the base types the blocks it
    // allocates by, so the two always agree.
    const int depth = CVTools::toOpencvType(blockDataType(channels));
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
    if (blockSize.width <= 0 || blockSize.height <= 0) {
        RAISE_RUNTIME_ERROR << "SourceCVScene: block size must be positive, got "
                            << blockSize.width << "x" << blockSize.height;
    }
    if (blockRect.width <= 0 || blockRect.height <= 0) {
        RAISE_RUNTIME_ERROR << "SourceCVScene: block rectangle must have positive size, got "
                            << blockRect.width << "x" << blockRect.height;
    }
    const std::vector<int> channels = validateRequest(componentIndices, zSliceIndex, tFrameIndex);
    cv::Mat mat = allocateBlock(blockSize, channels, output);
    m_source->readBlock(Rect(blockRect), Size(blockSize.width, blockSize.height),
                        channels, zSliceIndex, tFrameIndex,
                        mat.data, mat.total() * mat.elemSize());
}

void SourceCVScene::readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
    const cv::Size& blockSize, const std::vector<int>& channelIndices,
    int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
    validateLevel(level);
    const std::vector<int> channels = validateRequest(channelIndices, zSliceIndex, tFrameIndex);

    // An empty rectangle or block is not an error on the level path: the base
    // contract (readClampedBlock) answers it with a background block, and the
    // fallback below delivers exactly that. Only a real read goes to the source.
    const bool hasPixels = blockSize.width > 0 && blockSize.height > 0
                           && levelRect.width > 0 && levelRect.height > 0;
    if (hasPixels) {
        cv::Mat mat = allocateBlock(blockSize, channels, output);
        if (m_source->readLevelBlock(level, Rect(levelRect),
                Size(blockSize.width, blockSize.height), channels,
                zSliceIndex, tFrameIndex, mat.data, mat.total() * mat.elemSize())) {
            return;
        }
    }
    // The source has no pyramid of its own. The base clamps the rectangle to
    // the level, maps it to scene coordinates through LevelInfo::getScale()
    // and comes back through readResampledBlockChannelsEx. Every block it
    // allocates is typed by the requested channels, so output -- already of
    // that size and type -- is written in place rather than reallocated.
    CVScene::readResampledLevelBlockChannelsEx(level, levelRect, blockSize, channels,
                                               zSliceIndex, tFrameIndex, output);
}

bool SourceCVScene::supportsConcurrentReads() const
{
    return m_source->supportsConcurrentReads();
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

std::shared_ptr<CVScene> SourceCVScene::getAuxImage(const std::string& imageName) const
{
    const std::shared_ptr<Scene> image = m_source->getAuxImage(imageName);
    if (!image) {
        RAISE_RUNTIME_ERROR << "SourceCVScene: the source has no auxiliary image '"
                            << imageName << "'";
    }
    return SceneInternal::getCVScene(image);
}
