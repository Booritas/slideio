// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "vsifilescene.hpp"

using namespace slideio;

VsiFileScene::VsiFileScene(const std::string& filePath, std::shared_ptr<vsi::VSIFile>& vsiFile, int directoryIndex) :
    VSIScene(filePath, vsiFile), m_directoryIndex(directoryIndex)
{
    init();
}

void VsiFileScene::readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
                                              const std::vector<int>& channelIndices, cv::OutputArray output)
{
}

int VsiFileScene::getTileCount(void* userData)
{
    return 0;
}

bool VsiFileScene::getTileRect(int tileIndex, cv::Rect& tileRect, void* userData)
{
    return false;
}

bool VsiFileScene::readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
                            void* userData)
{
    return false;
}

void VsiFileScene::init()
{
    SLIDEIO_LOG(INFO) << "VSIImageDriver initialization of a vsi scene";
    if (!m_vsiFile) {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: vsi file is not initialized";
    }
    const int numTiffDirectories = m_vsiFile->getNumTiffDirectories();
    if (m_directoryIndex < 0 || m_directoryIndex > numTiffDirectories) {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: directory index " << m_directoryIndex
            << " is out of range (0-" << numTiffDirectories << ")";
    }
    const TiffDirectory& directory = m_vsiFile->getTiffDirectory(m_directoryIndex);
    m_rect = cv::Rect(0, 0, directory.width, directory.height);
    m_numChannels = directory.channels;
    m_channelDataType.resize(m_numChannels);
    m_channelNames.resize(m_numChannels);
    std::fill(m_channelDataType.begin(), m_channelDataType.end(), directory.dataType);
    m_compression = directory.slideioCompression;
}
