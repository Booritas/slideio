// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "vsifilescene.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/base/log.hpp"

#include <opencv2/imgproc.hpp>

using namespace slideio;
using namespace slideio::vsi;

VsiFileScene::VsiFileScene(const std::string& filePath, std::shared_ptr<vsi::VSIFile>& vsiFile, int directoryIndex) :
    VSIScene(filePath, vsiFile), m_directoryIndex(directoryIndex)
{
    init();
}

void VsiFileScene::readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
                                              const std::vector<int>& channelIndices, cv::OutputArray output)
{
    const TiffDirectory& directory = m_vsiFile->getTiffDirectory(m_directoryIndex);
    if(!directory.tiled) {
        cv::Mat directoryRaster;
        TiffTools::readStripedDir(m_tiff, directory, directoryRaster);
        cv::Mat blockRaster(directoryRaster, blockRect);
        cv::Mat resizedBlockRaster;
        cv::resize(blockRaster, resizedBlockRaster, blockSize);
        Tools::extractChannels(resizedBlockRaster, channelIndices, output);
    } else {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: Tiled images are not implemented";
    }
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
    m_tiff = TiffTools::openTiffFile(m_filePath);

    m_levels.resize(1);
    LevelInfo& level = m_levels[0];
    const Size rectSize(m_rect.width, m_rect.height);
    level.setLevel(0);
    level.setTileSize(rectSize);
    level.setSize(rectSize);
    level.setMagnification(getMagnification());
    level.setScale(1.);
}
