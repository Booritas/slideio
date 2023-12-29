// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "slideio/drivers/ndpi/ndpiscene.hpp"

#include <opencv2/imgproc.hpp>

#include "ndpifile.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/drivers/ndpi/ndpitiffmessagehandler.hpp"
#include "slideio/imagetools/imagetools.hpp"

using namespace slideio;

class NDPIUserData
{
public:
    NDPIUserData(const NDPITiffDirectory* dir, const std::string& filePath) : m_dir(dir),
                                                                              m_file(nullptr),
                                                                              m_filePath(filePath)
    {
        if ((!dir->tiled) && (dir->rowsPerStrip == dir->height) 
            && (dir->slideioCompression==Compression::Jpeg
            || dir->slideioCompression==Compression::Uncompressed)) {
            m_file = Tools::openFile(filePath, "rb");
            if (!m_file) {
                RAISE_RUNTIME_ERROR << "NDPI Image Driver: Cannot open file " << filePath;
            }
        }
    }

    ~NDPIUserData()
    {
        if (m_file) {
            fclose(m_file);
        }
    }

    const NDPITiffDirectory* dir() const
    {
        return m_dir;
    }

    FILE* file() const
    {
        return m_file;
    }

    const std::string& filePath() const
    {
        return m_filePath;
    }

private:
    const NDPITiffDirectory* m_dir;
    FILE* m_file;
    std::string m_filePath;
};

NDPIScene::NDPIScene() : m_pfile(nullptr), m_startDir(-1), m_endDir(-1), m_rect(0, 0, 0, 0)
{
}

NDPIScene::~NDPIScene()
{
}

void NDPIScene::init(const std::string& name, NDPIFile* file, int32_t startDirIndex, int32_t endDirIndex)
{
    NDPITIFFMessageHandler mh;

    m_sceneName = name;
    m_pfile = file;
    m_startDir = startDirIndex;
    m_endDir = endDirIndex;

    if (!m_pfile) {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid file handle.";
    }
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    if (m_startDir < 0 || m_startDir >= directories.size()) {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->
            getFilePath();
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
    if (m_startDir < 0 || m_startDir >= directories.size()) {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->
            getFilePath();
    }
    const auto& dir = directories[m_startDir];
    return dir.channels;
}


std::string NDPIScene::getFilePath() const
{
    if (!m_pfile) {
        throw std::runtime_error(std::string("NDPIScene: Invalid file pointer"));
    }
    return m_pfile->getFilePath();
}

slideio::DataType NDPIScene::getChannelDataType(int channel) const
{
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    if (m_startDir < 0 || m_startDir >= directories.size()) {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->
            getFilePath();
    }
    const auto& dir = directories[m_startDir];
    return dir.dataType;
}

Resolution NDPIScene::getResolution() const
{
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    if (m_startDir < 0 || m_startDir >= directories.size()) {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->
            getFilePath();
    }
    const auto& dir = directories[m_startDir];
    return dir.res;
}

double NDPIScene::getMagnification() const
{
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    if (m_startDir < 0 || m_startDir >= directories.size()) {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->
            getFilePath();
    }
    const auto& dir = directories[m_startDir];
    return dir.magnification;
}

Compression NDPIScene::getCompression() const
{
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    if (m_startDir < 0 || m_startDir >= directories.size()) {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->
            getFilePath();
    }
    const auto& dir = directories[m_startDir];
    return dir.slideioCompression;
}


const NDPITiffDirectory& NDPIScene::findZoomDirectory(const cv::Rect& imageBlockRect, const cv::Size& requiredBlockSize) const
{
    auto directories = m_pfile->directories();

    const double zoomImageToBlockX = static_cast<double>(requiredBlockSize.width) / static_cast<double>(imageBlockRect.width);
    const double zoomImageToBlockY = static_cast<double>(requiredBlockSize.height) / static_cast<double>(imageBlockRect.height);

    const double zoom = std::max(zoomImageToBlockX, zoomImageToBlockY);
    const NDPITiffDirectory& dir = m_pfile->findZoomDirectory(zoom, m_rect.width, m_startDir, m_endDir);
    return dir;
}

void NDPIScene::scaleBlockToDirectory(const cv::Rect& imageBlockRect, const slideio::NDPITiffDirectory& dir, cv::Rect& dirBlockRect) const
{
    // scale coefficients to scale original image to the directory image
    const double zoomImageToDirX = static_cast<double>(dir.width) / static_cast<double>(m_rect.width);
    const double zoomImageToDirY = static_cast<double>(dir.height) / static_cast<double>(m_rect.height);

    // rectangle on the directory zoom level
    Tools::scaleRect(imageBlockRect, zoomImageToDirX, zoomImageToDirY, dirBlockRect);
}

void NDPIScene::readResampledBlockChannels(const cv::Rect& imageBlockRect, const cv::Size& requiredBlockSize,
                                           const std::vector<int>& channelIndices, cv::OutputArray output)
{

    const slideio::NDPITiffDirectory& dir = findZoomDirectory(imageBlockRect, requiredBlockSize);
    const auto& directories = m_pfile->directories();

    cv::Rect dirBlockRect;
    scaleBlockToDirectory(imageBlockRect, dir, dirBlockRect);
    NDPITiffTools::setCurrentDirectory(m_pfile->getTiffHandle(), dir);
    NDPIUserData data(&dir, getFilePath());
    const auto dirType = dir.getType();
    if(dirType == NDPITiffDirectory::Type::Tiled 
        || dirType == NDPITiffDirectory::Type::SingleStripeMCU
        || dirType == NDPITiffDirectory::Type::Striped ) {
               TileComposer::composeRect(this, channelIndices, dirBlockRect, requiredBlockSize, output, (void*)&data);
    } else if(dirType==NDPITiffDirectory::Type::SingleStripe){
        cv::Mat raster;
        NDPITiffTools::readStripedDir(m_pfile->getTiffHandle(), dir, raster);
        cv::Mat block(raster, dirBlockRect);
        cv::Mat blockResized;
        cv::resize(block, blockResized, requiredBlockSize);
        Tools::extractChannels(blockResized, channelIndices, output);
    } else {
        RAISE_RUNTIME_ERROR << "NDPIScene::readResampledBlockChannels: Unexpected directory type: " << dir.getType();
    }
}

int NDPIScene::getTileCount(void* userData)
{
    const NDPIUserData* data = static_cast<const NDPIUserData*>(userData);

    const NDPITiffDirectory* dir = data->dir();
    const auto directoryType = dir->getType();

    switch(directoryType) {
        case NDPITiffDirectory::Type::Tiled:
        case NDPITiffDirectory::Type::SingleStripeMCU: {
            const int tilesX = (dir->width - 1) / dir->tileWidth + 1;
            const int tilesY = (dir->height - 1) / dir->tileHeight + 1;
            return tilesX * tilesY;
        }
        case NDPITiffDirectory::Type::Striped: {
            const int stripes = (dir->height - 1) / dir->rowsPerStrip + 1;
            return stripes;
        }
    }

    RAISE_RUNTIME_ERROR << "NDPIScene::getTileCount: Invalid directory type " << dir->getType();
    return 0;
}

bool NDPIScene::getTileRect(int tileIndex, cv::Rect& tileRect, void* userData)
{
    NDPITIFFMessageHandler mh;

    const NDPIUserData* data = static_cast<const NDPIUserData*>(userData);
    const NDPITiffDirectory* dir = data->dir();
    switch (dir->getType()) {
        case NDPITiffDirectory::Type::Tiled:
        case NDPITiffDirectory::Type::SingleStripeMCU: {
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
        case NDPITiffDirectory::Type::Striped: {
            const int rowsPerStrip = dir->rowsPerStrip;
            const int y = tileIndex * rowsPerStrip;
            tileRect.x = 0;
            tileRect.y = y;
            tileRect.width = dir->width;
            tileRect.height = NDPITiffTools::computeStripHeight(dir->height, rowsPerStrip, tileIndex);
            return true;
        }
    default:
        RAISE_RUNTIME_ERROR << "NDPIScene::getTileRect: Unexpected directory type: " << dir->getType();
        break;
    }
    return false;
}

void NDPIScene::makeSureValidDirectoryType(NDPITiffDirectory::Type directoryType) {
    switch (directoryType) {
    case NDPITiffDirectory::Type::Tiled:
    case NDPITiffDirectory::Type::SingleStripe:
    case NDPITiffDirectory::Type::SingleStripeMCU:
    case NDPITiffDirectory::Type::Striped:
        break;
    default:
        RAISE_RUNTIME_ERROR << "NDPIScene::readTile: Unexpected directory type: " << directoryType;
        break;
    }
}

bool NDPIScene::readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
                         void* userData)
{
    NDPITIFFMessageHandler mh;

    const NDPIUserData* data = static_cast<const NDPIUserData*>(userData);
    const NDPITiffDirectory* dir = data->dir();
    bool ret = false;
    NDPITiffDirectory::Type directoryType = dir->getType();

    makeSureValidDirectoryType(directoryType);

    try {
        switch (directoryType) {
        case NDPITiffDirectory::Type::Tiled: {
            NDPITiffTools::readTile(m_pfile->getTiffHandle(), *dir, tileIndex, channelIndices, tileRaster);
            ret = true;
            break;
        }
        case NDPITiffDirectory::Type::SingleStripeMCU: {
            cv::Mat stripRaster;
            NDPITiffTools::readMCUTile(data->file(), *dir, tileIndex, stripRaster);
            Tools::extractChannels(stripRaster, channelIndices, tileRaster);
            ret = true;
            break;
        }
        case NDPITiffDirectory::Type::Striped: {
            NDPITiffTools::readStripe(m_pfile->getTiffHandle(), *dir, tileIndex, channelIndices, tileRaster);
            ret = true;
            break;
        }
        case NDPITiffDirectory::Type::SingleStripe: {
            cv::Mat raster;
            NDPITiffTools::readStripedDir(m_pfile->getTiffHandle(), *dir, raster);
            cv::Rect tileRect;
            if(getTileRect(tileIndex,tileRect, userData)) {
                cv::Mat blockRaster(raster, tileRect);
                Tools::extractChannels(blockRaster, channelIndices, tileRaster);
                ret = true;
            }
            break;
        }
        }
    }
    catch (std::runtime_error& ) {
        SLIDEIO_LOG(WARNING) << "NDPIScene::readTile: Cannot read tile " << tileIndex
            << " from directory " << dir->dirIndex << ".Directory type: " << dir->getType();
    }

    return ret;
}

void NDPIScene::initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices, cv::OutputArray output)
{
    initializeSceneBlock(blockSize, channelIndices, output);
}
