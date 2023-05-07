// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/svs//svstiledscene.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/drivers/svs/svstools.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/drivers/svs/svsscene.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/imagetools/cvtools.hpp"

using namespace slideio;


SVSTiledScene::SVSTiledScene(const std::string& filePath, const std::string& name, 
    const std::vector<TiffDirectory>& dirs): SVSScene(filePath, name), m_directories(dirs)
{
    initialize();
}

SVSTiledScene::SVSTiledScene(const std::string& filePath, libtiff::TIFF* hFile, const std::string& name,
    const std::vector<slideio::TiffDirectory>& dirs) : SVSScene(filePath, hFile, name), m_directories(dirs)
{
    initialize();
}

void SVSTiledScene::initialize()
{
    m_resolution = { 0., 0. };
    auto& dir = m_directories[0];
    m_dataType = dir.dataType;

    if (m_dataType == DataType::DT_None || m_dataType == DataType::DT_Unknown)
    {
        switch (dir.bitsPerSample)
        {
        case 8:
            m_dataType = dir.dataType = DataType::DT_Byte;
            break;
        case 16:
            m_dataType = dir.dataType = DataType::DT_UInt16;
            break;
        default:
            m_dataType = DataType::DT_Unknown;
        }
    }
    m_magnification = SVSTools::extractMagnifiation(dir.description);
    double res = SVSTools::extractResolution(dir.description);
    m_resolution = { res, res };
    if (!m_directories.empty())
    {
        const auto& dir = m_directories.front();
        m_compression = dir.slideioCompression;
        if (m_compression == Compression::Unknown &&
            (dir.compression == 33003 || dir.compression == 3305))
        {
            m_compression = Compression::Jpeg2000;
        }
    }
}


cv::Rect SVSTiledScene::getRect() const
{
    cv::Rect rect = { 0,0,  m_directories[0].width,  m_directories[0].height };
    return rect;
}

int SVSTiledScene::getNumChannels() const
{
    const auto& dir = m_directories[0];
    return dir.channels;
}


void SVSTiledScene::readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& channelIndices, cv::OutputArray output)
{
    auto hFile = getFileHandle();
    if (hFile == nullptr)
        throw std::runtime_error("SVSDriver: Invalid file header by raster reading operation");
    double zoomX = static_cast<double>(blockSize.width) / static_cast<double>(blockRect.width);
    double zoomY = static_cast<double>(blockSize.height) / static_cast<double>(blockRect.height);
    double zoom = std::max(zoomX, zoomY);
    const slideio::TiffDirectory& dir = findZoomDirectory(zoom);
    double zoomDirX = static_cast<double>(dir.width) / static_cast<double>(m_directories[0].width); 
    double zoomDirY = static_cast<double>(dir.height) / static_cast<double>(m_directories[0].height);
    cv::Rect resizedBlock;
    Tools::scaleRect(blockRect, zoomDirX, zoomDirY, resizedBlock);
    TileComposer::composeRect(this, channelIndices, resizedBlock, blockSize, output, (void*)&dir);
}

const TiffDirectory& SVSTiledScene::findZoomDirectory(double zoom) const
{
    const cv::Rect sceneRect = getRect();
    const double sceneWidth = static_cast<double>(sceneRect.width);
    const auto& directories = m_directories;
    int index = Tools::findZoomLevel(zoom, (int)m_directories.size(), [&directories, sceneWidth](int index){
        return directories[index].width/sceneWidth;
    });
    return m_directories[index];
}

int SVSTiledScene::getTileCount(void* userData)
{
    const TiffDirectory* dir = (const TiffDirectory*)userData;
    int tilesX = (dir->width-1)/dir->tileWidth + 1;
    int tilesY = (dir->height-1)/dir->tileHeight + 1;
    return tilesX * tilesY;
}

bool SVSTiledScene::getTileRect(int tileIndex, cv::Rect& tileRect, void* userData)
{
    const TiffDirectory* dir = (const TiffDirectory*)userData;
    const int tilesX = (dir->width - 1) / dir->tileWidth + 1;
    const int tilesY = (dir->height - 1) / dir->tileHeight + 1;
    const int tileY = tileIndex / tilesX;
    const int tileX = tileIndex % tilesX;
    tileRect.x = tileX * dir->tileWidth;
    tileRect.y = tileY * dir->tileHeight;
    tileRect.width = dir->tileWidth;
    tileRect.height = dir->tileHeight;
    return true;
}

bool SVSTiledScene::readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
    void* userData)
{
    const TiffDirectory* dir = (const TiffDirectory*)userData;
    bool ret = false;
    try
    {
        TiffTools::readTile(getFileHandle(), *dir, tileIndex, channelIndices, tileRaster);
        ret = true;
    }
    catch(std::runtime_error&){
        const cv::Size tileSize = { dir->tileWidth, dir->tileHeight };
        const slideio::DataType dt = dir->dataType;
        tileRaster.create(tileSize, CV_MAKETYPE(slideio::CVTools::toOpencvType(dt), dir->channels));
        tileRaster.setTo(0);
    }

    return ret;
}

void SVSTiledScene::initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices, cv::OutputArray output)
{
    initializeSceneBlock(blockSize, channelIndices, output);
}

