// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "slideio/drivers/ndpi/ndpiscene.hpp"

#include <opencv2/imgproc.hpp>
#include <nlohmann/json.hpp>
#include <sstream>
#include <cmath>

#include "ndpifile.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/drivers/ndpi/ndpitiffmessagehandler.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/base/log.hpp"

using namespace slideio;

namespace
{
    template <typename T>
    std::string enumToString(const T& value)
    {
        std::ostringstream os;
        os << value;
        return os.str();
    }

    nlohmann::json directoryToJson(const NDPITiffDirectory& dir)
    {
        using nlohmann::json;
        json j;
        j["dirIndex"] = dir.dirIndex;
        j["offset"] = dir.offset;
        j["width"] = dir.width;
        j["height"] = dir.height;
        j["tiled"] = dir.tiled;
        j["tileWidth"] = dir.tileWidth;
        j["tileHeight"] = dir.tileHeight;
        j["channels"] = dir.channels;
        j["bitsPerSample"] = dir.bitsPerSample;
        j["photometric"] = dir.photometric;
        j["YCbCrSubsampling"] = { dir.YCbCrSubsampling[0], dir.YCbCrSubsampling[1] };
        j["compression"] = dir.compression;
        j["slideioCompression"] = enumToString(dir.slideioCompression);
        j["description"] = dir.description;
        j["userLabel"] = dir.userLabel;
        j["comments"] = dir.comments;
        j["resolution"] = { {"x", dir.res.x}, {"y", dir.res.y} };
        j["position"] = { {"x", dir.position.x}, {"y", dir.position.y} };
        j["interleaved"] = dir.interleaved;
        j["rowsPerStrip"] = dir.rowsPerStrip;
        j["dataType"] = enumToString(dir.dataType);
        j["stripSize"] = dir.stripSize;
        j["magnification"] = dir.magnification;
        j["blankLines"] = dir.blankLines;
        j["mcuStartsCount"] = static_cast<uint64_t>(dir.mcuStarts.size());
        j["jpegHeaderOffset"] = dir.jpegHeaderOffset;
        j["jpegSOFMarker"] = dir.jpegSOFMarker;
        j["jpegHeaderSize"] = dir.jpegHeaderSize;
        j["rawStripSize"] = dir.rawStripSize;
        j["auxImage"] = dir.auxImage;
        j["type"] = enumToString(dir.getType());

        if (!dir.subdirectories.empty()) {
            auto subs = json::array();
            for (const auto& sub : dir.subdirectories) {
                subs.push_back(directoryToJson(sub));
            }
            j["subdirectories"] = subs;
        }
        return j;
    }
}

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

NDPIScene::NDPIScene() : m_pfile(nullptr), m_startDir(-1), m_endDir(-1), m_rect(0, 0, 0, 0), m_sceneIndex(-1)
{
}

NDPIScene::~NDPIScene()
{
}

void NDPIScene::init(const std::string& name, int sceneIndex, const std::string& driverId, NDPIFile* file, int32_t startDirIndex, int32_t endDirIndex)
{
    NDPITIFFMessageHandler mh;

    m_sceneName = name;
    m_pfile = file;
    m_startDir = startDirIndex;
    m_endDir = endDirIndex;
	m_sceneIndex = sceneIndex;
	m_driverId = driverId;

    if (!m_pfile) {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid file handle.";
    }
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    if (m_startDir < 0 || m_startDir >= directories.size()) {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Invalid directory index: " << m_startDir << ". File:" << m_pfile->
            getFilePath();
    }
    const NDPITiffDirectory& dir = directories[m_startDir];
    m_rect.width = dir.width;
    m_rect.height = dir.height;

    m_rawMetadata = directoryToJson(dir).dump(2);
    m_metadataFormat = MetadataFormat::JSON;

    const int numLevels = m_endDir - m_startDir;
    m_levels.resize(numLevels);
    for(int lv = 0; lv < numLevels; ++lv) {
        const NDPITiffDirectory& directory = directories[m_startDir + lv];
        LevelInfo& level = m_levels[lv];
        const double scale = static_cast<double>(directory.width) / static_cast<double>(m_rect.width);
        level.setLevel(lv);
        level.setScale(scale);
        level.setSize({ directory.width, directory.height });
        level.setTileSize({ directory.tileWidth, directory.tileHeight });
        level.setMagnification(getMagnification() * scale);
    }
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

void NDPIScene::readResampledBlockChannelsEx(const cv::Rect& imageBlockRect, const cv::Size& requiredBlockSize,
        const std::vector<int>& channelIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
    const double zoomX = static_cast<double>(requiredBlockSize.width) / static_cast<double>(imageBlockRect.width);
    const double zoomY = static_cast<double>(requiredBlockSize.height) / static_cast<double>(imageBlockRect.height);
    const int level = findZoomLevelIndex(std::max(zoomX, zoomY));
    const auto& directories = m_pfile->directories();
    const slideio::NDPITiffDirectory& dir = directories[m_startDir + level];

    cv::Rect dirBlockRect;
    scaleBlockToDirectory(imageBlockRect, dir, dirBlockRect);
    readResampledLevelBlockChannelsEx(level, dirBlockRect, requiredBlockSize, channelIndices,
                                      zSliceIndex, tFrameIndex, output);
}

void NDPIScene::readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
        const cv::Size& blockSize, const std::vector<int>& channelIndices,
        int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
    if (zSliceIndex != 0 || tFrameIndex != 0) {
        RAISE_RUNTIME_ERROR << "NDPIScene: 3D and 4D images are not supported";
    }
    validateLevel(level);
    const auto& directories = m_pfile->directories();
    const slideio::NDPITiffDirectory& dir = directories[m_startDir + level];

    NDPITiffTools::setCurrentDirectory(m_pfile->getTiffHandle(), dir);
    NDPIUserData data(&dir, getFilePath());
    const auto dirType = dir.getType();
    if (dirType == NDPITiffDirectory::Type::Tiled
        || dirType == NDPITiffDirectory::Type::SingleStripeMCU
        || dirType == NDPITiffDirectory::Type::Striped) {
        // TileComposer intersects every tile with the block, so an overhanging block simply
        // finds no tile there and keeps the background.
        TileComposer::composeRect(this, channelIndices, levelRect, blockSize, output, (void*)&data);
    } else if (dirType == NDPITiffDirectory::Type::SingleStripe) {
        cv::Mat raster;
        NDPITiffTools::readStripedDir(m_pfile->getTiffHandle(), dir, raster);
        // cv::Mat(raster, rect) throws unless the rect is contained, and an edge tile of a
        // level is not: clamp, read the part that exists, and leave the rest background.
        const cv::Rect valid = levelRect & cv::Rect(0, 0, raster.cols, raster.rows);
        initializeSceneBlock(blockSize, channelIndices, output);
        if (valid.empty()) {
            return;
        }
        const double scaleX = static_cast<double>(blockSize.width) / static_cast<double>(levelRect.width);
        const double scaleY = static_cast<double>(blockSize.height) / static_cast<double>(levelRect.height);
        cv::Rect target;
        target.x = static_cast<int>(std::floor((valid.x - levelRect.x) * scaleX));
        target.y = static_cast<int>(std::floor((valid.y - levelRect.y) * scaleY));
        target.width = std::min(static_cast<int>(std::ceil(valid.width * scaleX)), blockSize.width - target.x);
        target.height = std::min(static_cast<int>(std::ceil(valid.height * scaleY)), blockSize.height - target.y);
        if (target.width <= 0 || target.height <= 0) {
            return;
        }
        cv::Mat block(raster, valid);
        cv::Mat blockResized;
        cv::resize(block, blockResized, target.size());
        cv::Mat extracted;
        Tools::extractChannels(blockResized, channelIndices, extracted);
        cv::Mat out = output.getMat();
        extracted.copyTo(out(target));
    } else {
        RAISE_RUNTIME_ERROR << "NDPIScene::readResampledLevelBlockChannelsEx: Unexpected directory type: "
            << dir.getType();
    }
}

int NDPIScene::findZoomLevelIndex(double zoom) const
{
    // The body of NDPIFile::findZoomDirectory (ndpifile.cpp:55-63) without its final
    // index-to-reference step, so the level it names is the level that function would.
    const auto& directories = m_pfile->directories();
    const int sceneWidth = m_rect.width;
    const int begin = m_startDir;
    return Tools::findZoomLevel(zoom, m_endDir - m_startDir,
        [&directories, sceneWidth, begin](int index) {
            return static_cast<double>(directories[index + begin].width) / static_cast<double>(sceneWidth);
        });
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
