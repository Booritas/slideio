// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <set>
#include <algorithm>
#include <cmath>
#include <opencv2/imgproc.hpp>

#include "slideio/drivers/dcm/dcmscene.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/slideio_enums.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/core/log.hpp"


using namespace slideio;

DCMScene::DCMScene()
{
	m_metadataFormat = MetadataFormat::JSON;
}

std::string DCMScene::getFilePath() const
{
    return m_filePath;
}

cv::Rect DCMScene::getRect() const
{
    return m_rect;
}

int DCMScene::getNumChannels() const
{
    return m_numChannels;
}

int DCMScene::getNumZSlices() const
{
    return m_numSlices;
}

int DCMScene::getNumTFrames() const
{
    return m_numFrames;
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
    return m_dataType;
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

std::string DCMScene::getName() const
{
    return m_name;
}

Compression DCMScene::getCompression() const
{
    return m_compression;
}

void DCMScene::addFile(std::shared_ptr<DCMFile>& file)
{
    m_files.push_back(file);
}

void DCMScene::prepareSliceIndices()
{
    for (int index = 0; index < m_files.size(); ++index)
    {
        auto file = m_files[index];
        m_sliceMap[file->getInstanceNumber() - 1] = index;
    }
}

void DCMScene::checkScene()
{
    if (m_files.size() > 1)
    {
        int slices = 0;
        std::set<std::pair<int, int>> sizes;
        std::set<std::string> series;
        std::set<int> channelCounts;
        std::set<DataType> types;
        for (auto&& file : m_files)
        {
            sizes.insert({file->getWidth(), file->getHeight()});
            slices += file->getNumSlices();
            series.insert(file->getSeriesUID());
            channelCounts.insert(file->getNumChannels());
            types.insert(file->getDataType());
        }
        if (sizes.size() != 1)
        {
            RAISE_RUNTIME_ERROR << "DCMImageDriver: Attempt to create a scene with different slice sizes. Found "
                << sizes.size() << " different sizes";
        }

        if (series.size() != 1)
        {
            RAISE_RUNTIME_ERROR << "DCMImageDriver: Attempt to create a scene from different series. Found "
                << series.size() << " different series";
        }

        if (channelCounts.size() != 1)
        {
            RAISE_RUNTIME_ERROR <<
                "DCMImageDriver: Attempt to create a scene from slices with different number of channels.";
        }

        if (slices != static_cast<int>(m_files.size()))
        {
            RAISE_RUNTIME_ERROR <<
                "DCMImageDriver: Each file from multi-file scene shall have exactly 1 frame!";
        }

        if (types.size() > 1)
        {
            RAISE_RUNTIME_ERROR << "DCMImageDriver: Attempt to create a scene from files with different data types";
        }
    }
}

void DCMScene::init(const std::string& slideFilePath, int sceneIndex, const std::string& driverId)
{
    SLIDEIO_LOG(INFO) << "DCMScene::init-begin";
    if (m_files.empty())
    {
        RAISE_RUNTIME_ERROR << "DCMScene::init attempt to create an empty scene.";
    }

    m_filePath = slideFilePath;
	m_sceneIndex = sceneIndex;
    m_driverId = driverId;

    checkScene();

    const auto file = *(m_files.begin());

    m_rect = {0, 0, file->getWidth(), file->getHeight()};

    if (m_files.size() > 1)
    {
        m_numSlices = static_cast<int>(m_files.size());
    }
    else
    {
        m_numSlices = file->getNumSlices();
    }

    m_name = file->getSeriesUID();
    const std::string seriesDescription = (*(m_files.begin()))->getSeriesDescription();

    if (!seriesDescription.empty())
    {
        m_name = seriesDescription;
    }
    m_numChannels = file->getNumChannels();
    m_dataType = file->getDataType();
    m_compression = file->getCompression();

    m_rawMetadata = file->getMetadata();
    m_metadataFormat = MetadataFormat::JSON;

    // `file` is this scene's own first-added file, not a fixed one -- this reads
    // its ICC profile whether the scene is a plain multi-slice series (dcmslide.cpp)
    // or a WSI aux image (label/macro/localizer), which WSIScene::addFile constructs
    // as its own DCMScene and initializes through this same path.
    m_colorProfile = file->readColorProfile();

    prepareSliceIndices();

    // BitsStored describes a sample only where the samples are what is stored.
    // For a palette image it is the width of the index into the lookup table,
    // whose entries are 16 bit here while the index is 8 -- reporting 8 would
    // answer a question about a different number.
    if (file->getPhotointerpretation() != EPhotoInterpetation::PHIN_PALETTE) {
        m_significantBits = file->getBitsStored();
    }
    collectPlaneTimestamps();

    m_levels.resize(1);
    LevelInfo& level = m_levels[0];
    Size rectSize(m_rect.width, m_rect.height);
    level.setLevel(0);
    level.setTileSize(rectSize);
    level.setSize(rectSize);
    level.setMagnification(getMagnification());
    level.setScale(1.);
}

int DCMScene::getChannelSignificantBits(int channelIndex) const
{
    if (channelIndex < 0 || channelIndex >= m_numChannels) {
        return 0;
    }
    // Every channel of a DICOM image shares one BitsStored: the tag describes
    // the samples of the object, not of a component.
    return m_significantBits;
}

double DCMScene::getPlaneTimestamp(int tFrame, int channel, int zSlice) const
{
    if (m_planeTimestamps.empty()) {
        return 0.;
    }
    if (tFrame < 0 || tFrame >= m_numFrames || channel < 0 || channel >= m_numChannels
        || zSlice < 0 || zSlice >= static_cast<int>(m_planeTimestamps.size())) {
        return 0.;
    }
    // One file is one plane, and its channels are the samples of that plane.
    return m_planeTimestamps[zSlice];
}

void DCMScene::collectPlaneTimestamps()
{
    const auto file = *(m_files.begin());
    if (const auto& acquired = file->getAcquisitionTime()) {
        // The whole second, leaving the remainder in the plane offsets, so
        // getAcquisitionTime() + getPlaneTimestamp() is the plane's own instant.
        m_acquisitionTime = static_cast<int64_t>(std::floor(*acquired));
    }
    if (static_cast<int>(m_files.size()) != m_numSlices) {
        // One file holding several slices states one ContentTime for all of
        // them, which is a property of the object rather than of a plane. Only
        // a series of one file per slice carries a real per-plane time.
        return;
    }
    std::vector<double> absolute(m_numSlices, 0.);
    for (int slice = 0; slice < m_numSlices; ++slice) {
        int fileIndex = 0;
        if (m_files.size() > 1) {
            // The slice map rather than findFileIndex(), which throws where a
            // slice has no file of its own. InstanceNumber is what the map is
            // built from and nothing guarantees it runs 1..N, so throwing here
            // would turn a series with gaps in it from one without plane times
            // into one that cannot be opened at all.
            const auto it = m_sliceMap.find(slice);
            if (it == m_sliceMap.end()) {
                return;
            }
            fileIndex = it->second;
        }
        const auto& content = m_files[fileIndex]->getContentTime();
        if (!content) {
            // Partial coverage counts as none: the result is addressed by
            // plane, so a gap would leave one plane reporting another's time.
            return;
        }
        absolute[slice] = *content;
    }
    const double earliest = *std::min_element(absolute.begin(), absolute.end());
    double origin = static_cast<double>(m_acquisitionTime);
    if (!file->getAcquisitionTime() || earliest < origin) {
        // Either the series states no acquisition start, or it states one later
        // than its first plane, which CVScene::getPlaneTimestamp() forbids. The
        // earliest plane is then the origin, and no acquisition time is
        // reported: rebasing keeps the offsets non-negative, but the two
        // getters would no longer add up.
        if (file->getAcquisitionTime()) {
            SLIDEIO_LOG(WARNING) << "DCMImageDriver: " << m_filePath
                << " states a plane earlier than its acquisition time; reporting the"
                   " times relative to the earliest plane and no acquisition time";
        }
        m_acquisitionTime = 0;
        origin = earliest;
    }
    m_planeTimestamps.resize(m_numSlices);
    for (int slice = 0; slice < m_numSlices; ++slice) {
        m_planeTimestamps[slice] = absolute[slice] - origin;
    }
}

void DCMScene::extractSliceRaster(const cv::Mat& frame,
                                  const cv::Rect& blockRect,
                                  const cv::Size& blockSize,
                                  const std::vector<int>& componentIndices,
                                  cv::OutputArray output)
{
    cv::Mat block = frame(blockRect);
    cv::Mat resizedBlock;
    cv::resize(block, resizedBlock, blockSize);
    if (componentIndices.empty() || (componentIndices.size() == getNumChannels()
        && getNumChannels() == 1))
    {
        resizedBlock.copyTo(output);
    }
    else
    {
        std::vector<int> channelIndices = Tools::completeChannelList(componentIndices, getNumChannels());
        std::vector<cv::Mat> channelRasters(channelIndices.size());
        for (int index = 0; index < channelIndices.size(); ++index)
        {
            int channelIndex = channelIndices[index];
            cv::extractChannel(resizedBlock, channelRasters[index],
                               channelIndex);
        }
        cv::merge(channelRasters, output);
    }
}

std::pair<int, int> DCMScene::findFileIndex(int zSliceIndex)
{
    int fileIndex = 0;
    int fileSlice = zSliceIndex;
    if (m_files.size() > 1)
    {
        auto itSlice = m_sliceMap.find(zSliceIndex);
        if (itSlice == m_sliceMap.end())
        {
            RAISE_RUNTIME_ERROR << "DCMImageDriver: cannot find slice " <<
                zSliceIndex << ". file: " << m_filePath;
        }
        fileIndex = itSlice->second;
        fileSlice = 0;
    }
    std::pair<int, int> res(fileIndex, fileSlice);
    return res;
}

void DCMScene::readResampledBlockChannelsEx(const cv::Rect& blockRect,
    const cv::Size& blockSize,
    const std::vector<int>&
    componentIndices,
    int zSliceIndex,
    int tFrameIndex,
    cv::OutputArray output)
{
    SLIDEIO_LOG(INFO) << "DCMImageDriver: Resample block:" << std::endl
        << "block: " << blockRect.x << "," << blockRect.y << ","
        << blockRect.width << "," << blockRect.height << std::endl
        << "size: " << blockSize.width << "," << blockSize.height << std::endl
        << "channels:" << componentIndices.size() << std::endl
        << "slice: " << zSliceIndex << std::endl
        << "frame: " << tFrameIndex;

    const auto indices = findFileIndex(zSliceIndex);
    const int fileIndex = indices.first;
    const int fileSlice = indices.second;
    readClampedBlock(m_rect, blockRect, blockSize, componentIndices,
        [&](const cv::Rect& rect, const cv::Size& size, cv::OutputArray block) {
            auto file = m_files[fileIndex];
            std::vector<cv::Mat> frames;
            file->readPixelValues(frames, fileSlice, 1);
            extractSliceRaster(frames[0], rect, size, componentIndices, block);
        },
        output);
}