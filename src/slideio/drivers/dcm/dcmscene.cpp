// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <set>
#include <opencv2/imgproc.hpp>

#include "slideio/drivers/dcm/dcmscene.hpp"
#include "slideio/base/base.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/base/log.hpp"


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

void DCMScene::init()
{
    SLIDEIO_LOG(INFO) << "DCMScene::init-begin";
    if (m_files.empty())
    {
        RAISE_RUNTIME_ERROR << "DCMScene::init attempt to create an empty scene.";
    }

    m_filePath = (*m_files.begin())->getFilePath();

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

    prepareSliceIndices();

    m_levels.resize(1);
    LevelInfo& level = m_levels[0];
    Size rectSize(m_rect.width, m_rect.height);
    level.setLevel(0);
    level.setTileSize(rectSize);
    level.setSize(rectSize);
    level.setMagnification(getMagnification());
    level.setScale(1.);
}

std::string DCMScene::getRawMetadata() const
{
    std::string metadata;
    if(!m_files.empty()) {
        std::shared_ptr<DCMFile> file = m_files.front();
        metadata = file->getMetadata();
    }
    return metadata;
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
    auto file = m_files[fileIndex];
    std::vector<cv::Mat> frames;
    file->readPixelValues(frames, fileSlice, 1);
    extractSliceRaster(frames[0], blockRect, blockSize, componentIndices, output);
}