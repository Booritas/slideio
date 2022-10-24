// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "slideio/drivers/ndpi//ndpiscene.hpp"

#include "ndpifile.hpp"
#include "slideio/core/tools/tools.hpp"

using namespace slideio;

NDPIScene::NDPIScene() : m_pfile(nullptr), m_startDir(-1), m_endDir(-1), m_rect(0,0,0,0)
{
}

NDPIScene::~NDPIScene()
{
}

void NDPIScene::init(const std::string& name, NDPIFile* file, int32_t startDirIndex, int32_t endDirIndex)
{
    m_sceneName = name;
    m_pfile = file;
    m_startDir = startDirIndex;
    m_endDir = endDirIndex;

    if (!m_pfile) {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid file handle.";
    }
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    if (m_startDir < 0 || m_startDir >= directories.size()) {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->getFilePath();
    }
    const NDPITiffDirectory& dir = m_pfile->directories()[m_startDir];
    m_rect.width = dir.width;
    m_rect.height = dir.height;
}

cv::Rect NDPIScene::getRect() const
{
    return m_rect;
}

int NDPIScene::getNumChannels() const
{
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    if (m_startDir < 0 || m_startDir >= directories.size())
    {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->getFilePath();
    }
    const auto& dir = directories[m_startDir];
    return dir.channels;
}


std::string NDPIScene::getFilePath() const
{
    if (!m_pfile)
    {
        throw std::runtime_error(std::string("NDPIScene: Invalid file pointer"));
    }
    return m_pfile->getFilePath();
}

slideio::DataType NDPIScene::getChannelDataType(int channel) const
{
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    if (m_startDir < 0 || m_startDir >= directories.size())
    {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->getFilePath();
    }
    const auto& dir = directories[m_startDir];
    return dir.dataType;
}

Resolution NDPIScene::getResolution() const
{
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    if (m_startDir < 0 || m_startDir >= directories.size())
    {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->getFilePath();
    }
    const auto& dir = directories[m_startDir];
    return dir.res;
}

double NDPIScene::getMagnification() const
{
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    if (m_startDir < 0 || m_startDir >= directories.size())
    {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->getFilePath();
    }
    const auto& dir = directories[m_startDir];
    return dir.magnification;
}

Compression NDPIScene::getCompression() const
{
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    if (m_startDir < 0 || m_startDir >= directories.size())
    {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->getFilePath();
    }
    const auto& dir = directories[m_startDir];
    return dir.slideioCompression;
}

void NDPIScene::readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& channelIndices, cv::OutputArray output)
{
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    double zoomX = static_cast<double>(blockSize.width) / static_cast<double>(blockRect.width);
    double zoomY = static_cast<double>(blockSize.height) / static_cast<double>(blockRect.height);
    double zoom = std::max(zoomX, zoomY);
    const slideio::NDPITiffDirectory& dir = m_pfile->findZoomDirectory(zoom, m_rect.width, m_startDir, m_endDir);
    double zoomDirX = static_cast<double>(dir.width) / static_cast<double>(directories[0].width);
    double zoomDirY = static_cast<double>(dir.height) / static_cast<double>(directories[0].height);
    cv::Rect resizedBlock;
    Tools::scaleRect(blockRect, zoomDirX, zoomDirY, resizedBlock);
    TileComposer::composeRect(this, channelIndices, resizedBlock, blockSize, output, (void*)&dir);
}

int NDPIScene::getTileCount(void* userData)
{
    const NDPITiffDirectory* dir = (const NDPITiffDirectory*)userData;
    if (dir->tiled) {
        int tilesX = (dir->width-1)/dir->tileWidth + 1;
        int tilesY = (dir->height-1)/dir->tileHeight + 1;
        return tilesX * tilesY;
    } else {
        const int stripes = (dir->height - 1) / dir->rowsPerStrip + 1;
        return stripes;
    }
}

bool NDPIScene::getTileRect(int tileIndex, cv::Rect& tileRect, void* userData)
{
    const NDPITiffDirectory* dir = (const NDPITiffDirectory*)userData;
    if (dir->tiled) {
        const int tilesX = (dir->width - 1) / dir->tileWidth + 1;
        const int tilesY = (dir->height - 1) / dir->tileHeight + 1;
        const int tileY = tileIndex / tilesX;
        const int tileX = tileIndex % tilesX;
        tileRect.x = tileX * dir->tileWidth;
        tileRect.y = tileY * dir->tileHeight;
        tileRect.width = dir->tileWidth;
        tileRect.height = dir->tileHeight;
        return true;
    } else {
        const int y = tileIndex * dir->rowsPerStrip;
        tileRect.x = 0;
        tileRect.y = y;
        tileRect.width = dir->width;
        tileRect.height = NDPITiffTools::computeStripHeight(*dir, tileIndex);
        return true;
    }
}

bool NDPIScene::readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
    void* userData)
{
    const NDPITiffDirectory* dir = (const NDPITiffDirectory*)userData;
    bool ret = false;

    if (dir->tiled) {
        try
        {
            NDPITiffTools::readTile(m_pfile->getTiffHandle(), *dir, tileIndex, channelIndices, tileRaster);
            ret = true;
        }
        catch(std::runtime_error&){
        }
    } else {
        try
        {
            NDPITiffTools::readStrip(m_pfile->getTiffHandle(), *dir, tileIndex, channelIndices, tileRaster);
            ret = true;
        }
        catch (std::runtime_error&) {
        }
    }
    return ret;
}
