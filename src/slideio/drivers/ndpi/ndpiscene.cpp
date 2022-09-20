// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/ndpi//ndpiscene.hpp"

#include "ndpifile.hpp"

using namespace slideio;

NDPIScene::NDPIScene() : m_pfile(nullptr), m_startDir(-1), m_endDir(-1)
{
}

void NDPIScene::init(const std::string& name, NDPIFile* file, int32_t startDirIndex, int32_t endDirIndex)
{
    m_sceneName = name;
    m_pfile = file;
    m_startDir = startDirIndex;
    m_endDir = endDirIndex;

}

cv::Rect NDPIScene::getRect() const
{
    if (!m_pfile) 
    {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid file handle.";
    }
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    if(m_startDir<0 || m_startDir>=directories.size())
    {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->getFilePath();
    }
    const NDPITiffDirectory& dir = directories[m_startDir];
    cv::Rect rect(0,0, dir.width, dir.height);
    return rect;
}

int NDPIScene::getNumChannels() const
{
    // const auto& dir = m_directories[0];
    // return dir.channels;
    return -1;
}


void NDPIScene::readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& channelIndices, cv::OutputArray output)
{
    // auto hFile = getFileHandle();
    // if (hFile == nullptr)
    //     throw std::runtime_error("SVSDriver: Invalid file header by raster reading operation");
    // double zoomX = static_cast<double>(blockSize.width) / static_cast<double>(blockRect.width);
    // double zoomY = static_cast<double>(blockSize.height) / static_cast<double>(blockRect.height);
    // double zoom = std::max(zoomX, zoomY);
    // const slideio::TiffDirectory& dir = findZoomDirectory(zoom);
    // double zoomDirX = static_cast<double>(dir.width) / static_cast<double>(m_directories[0].width); 
    // double zoomDirY = static_cast<double>(dir.height) / static_cast<double>(m_directories[0].height);
    // cv::Rect resizedBlock;
    // ImageTools::scaleRect(blockRect, zoomDirX, zoomDirY, resizedBlock);
    // TileComposer::composeRect(this, channelIndices, resizedBlock, blockSize, output, (void*)&dir);
}

// const TiffDirectory& NDPIScene::findZoomDirectory(double zoom) const
// {
//     const cv::Rect sceneRect = getRect();
//     const double sceneWidth = static_cast<double>(sceneRect.width);
//     const auto& directories = m_directories;
//     int index = Tools::findZoomLevel(zoom, (int)m_directories.size(), [&directories, sceneWidth](int index){
//         return directories[index].width/sceneWidth;
//     });
//     return m_directories[index];
// }

int NDPIScene::getTileCount(void* userData)
{
    // const TiffDirectory* dir = (const TiffDirectory*)userData;
    // int tilesX = (dir->width-1)/dir->tileWidth + 1;
    // int tilesY = (dir->height-1)/dir->tileHeight + 1;
    // return tilesX * tilesY;
    return 0;
}

bool NDPIScene::getTileRect(int tileIndex, cv::Rect& tileRect, void* userData)
{
    // const TiffDirectory* dir = (const TiffDirectory*)userData;
    // const int tilesX = (dir->width - 1) / dir->tileWidth + 1;
    // const int tilesY = (dir->height - 1) / dir->tileHeight + 1;
    // const int tileY = tileIndex / tilesX;
    // const int tileX = tileIndex % tilesX;
    // tileRect.x = tileX * dir->tileWidth;
    // tileRect.y = tileY * dir->tileHeight;
    // tileRect.width = dir->tileWidth;
    // tileRect.height = dir->tileHeight;
    // return true;
    return false;
}

bool NDPIScene::readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
    void* userData)
{
    //const TiffDirectory* dir = (const TiffDirectory*)userData;
    bool ret = false;
    // try
    // {
    //     TiffTools::readTile(getFileHandle(), *dir, tileIndex, channelIndices, tileRaster);
    //     ret = true;
    // }
    // catch(std::runtime_error&){
    // }

    return ret;
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
    return DataType::DT_Unknown;
}

Resolution NDPIScene::getResolution() const
{
    Resolution res(0, 0);
    return res;
}

double NDPIScene::getMagnification() const
{
    return 0;
}

Compression NDPIScene::getCompression() const
{
    return Compression::Unknown;
}

