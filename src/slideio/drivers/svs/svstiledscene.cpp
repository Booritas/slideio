// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/svs//svstiledscene.hpp"

#include <algorithm>
#include <numeric>
#include <sstream>

#include "slideio/base/log.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/drivers/svs/svstools.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/drivers/svs/svsscene.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include <tinyxml2.h>

#include "phtdescription.hpp"
#include "slideio/drivers/svs/svsimagedriver.hpp"

using namespace slideio;


SVSTiledScene::SVSTiledScene(const std::string& filePath, const std::string& driverId, const std::string& name,
                             const std::vector<TiffDirectory>& dirs) : SVSScene(filePath, driverId, name),
                                                                       m_directories(dirs) {
    initialize();
}

SVSTiledScene::SVSTiledScene(const std::string& filePath, const std::string& driverId, libtiff::TIFF* hFile,
                             const std::string& name,
                             const std::vector<slideio::TiffDirectory>& dirs) : SVSScene(filePath, driverId, hFile,
    name), m_directories(dirs) {
    initialize();
}

void SVSTiledScene::processImageDescription() {
    if (m_driverId == PHTIFF_DRIVER_ID) {
        processImageDescriptionPhTiff();
    } else {
        processImageDescriptionSVS();
    }
}

void SVSTiledScene::processImageDescriptionSVS() {
    if (m_directories.empty()) {
        RAISE_RUNTIME_ERROR <<
            "SVSTiledScene::initialize: directories are empty, scene initialization may be incorrect";
    }
    auto& dir = m_directories.front();
    m_rawMetadata = SVSTools::tiffDirectoryToJson(dir).dump(2);
    m_metadataFormat = MetadataFormat::JSON;
    m_magnification = SVSTools::extractMagnifiation(dir.description);
    double res = SVSTools::extractResolution(dir.description);
    m_resolution = {res, res};
}

void SVSTiledScene::processImageDescriptionPhTiff() {
    if (m_directories.empty()) {
        RAISE_RUNTIME_ERROR <<
            "SVSTiledScene::processImageDescriptionPhTiff: directories are empty, cannot construct scene.";
    }
    auto& dir = m_directories.front();
    // The scene describes the tiff directory it reads from, exactly as the aperio branch
    // does. What the philips xml says about the slide belongs to the slide: it is one
    // document covering every image of the file, and 844 KB of it in Philips-2.tiff.
    m_rawMetadata = SVSTools::tiffDirectoryToJson(dir).dump(2);
    m_metadataFormat = MetadataFormat::JSON;

    // Philips names the magnification of every zoom level but the base, whose directory
    // carries the xml metadata instead. Any level gives the magnification of the slide,
    // but the value is stored rounded to six significant digits, so the first level that
    // names one -- the least deeply divided, and therefore the least rounded -- is used.
    for (const TiffDirectory& level : m_directories) {
        m_magnification = SVSTools::extractPhilipsMagnification(level.description);
        if (m_magnification > 0.) {
            break;
        }
    }
    if (m_magnification <= 0.) {
        SLIDEIO_LOG(WARNING) << "SVSTiledScene: no zoom level of the philips file names a"
            " magnification. The magnification of the scene is unknown.";
    }

    PHTDescription description(dir.description);

#if defined(_DEBUG)
    //std::string fileName = std::filesystem::path(m_filePath).stem().string();
    //std::string xmlPath = "D:/Temp/" + fileName + ".xml";
    //description.getDocument()->SaveFile(xmlPath.c_str());
#endif

    std::vector<tinyxml2::XMLElement*> images = description.getObjectList(description.getRoot(), SCANNED_IMAGES, SCANNED_IMAGE);
    for (const tinyxml2::XMLElement* image : images) {
        // An image the metadata declares without naming its type cannot be classified,
        // but it says nothing about the other images in the file. The order of the
        // attributes varies between scanners and so does which of them are present, so
        // an incomplete object is skipped rather than allowed to cost the caller the
        // whole slide.
        if (!description.hasAttribute(image, IMAGE_TYPE)) {
            SLIDEIO_LOG(WARNING) << "SVSTiledScene: a scanned image of the philips file declares"
                " no image type. The image is ignored.";
            continue;
        }
        const std::string imageType = description.getAttributeText(image, IMAGE_TYPE);
        if (imageType == WSI) {
            try {
                std::vector<double> resolutions = description.getAttributeDoubleList(image, IMAGE_RESOLUTION);
                if (resolutions.size() >= 2) {
                    m_resolution = { resolutions[0] * 1.e-3, resolutions[1] * 1.e-3 };
				}
				else {
                    SLIDEIO_LOG(WARNING) << "Unexpected DICOM_PIXEL_SPACING value count: " << resolutions.size();
				}
            } catch (std::exception& e) {
                SLIDEIO_LOG(WARNING) << "Cannot extract image resolution: " << e.what();
            }
        }
    }

}

void SVSTiledScene::initialize() {
    if (m_directories.empty()) {
        RAISE_RUNTIME_ERROR <<
            "SVSTiledScene::initialize: directories are empty, scene initialization may be incorrect";
    }
    m_resolution = {0., 0.};

    processImageDescription();

    auto& dir = m_directories[0];

    m_dataType = dir.dataType;
    if (m_dataType == DataType::DT_None || m_dataType == DataType::DT_Unknown) {
        switch (dir.bitsPerSample) {
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
    m_compression = dir.slideioCompression;
    if (m_compression == Compression::Unknown &&
        (dir.compression == 33003 || dir.compression == 3305)) {
        m_compression = Compression::Jpeg2000;
    }

    const int numLevels = static_cast<int>(m_directories.size());
    const int width0 = m_directories[0].width;
    m_levels.resize(m_directories.size());
    for (int lv = 0; lv < numLevels; ++lv) {
        const TiffDirectory& directory = m_directories[lv];
        LevelInfo& level = m_levels[lv];
        const double scale = static_cast<double>(directory.width) / static_cast<double>(width0);
        level.setLevel(lv);
        level.setScale(scale);
        level.setSize({directory.width, directory.height});
        level.setTileSize({directory.tileWidth, directory.tileHeight});
        level.setMagnification(m_magnification * scale);
    }
}



cv::Rect SVSTiledScene::getRect() const {
    cv::Rect rect = {0, 0, m_directories[0].width, m_directories[0].height};
    return rect;
}

int SVSTiledScene::getNumChannels() const {
    const auto& dir = m_directories[0];
    return dir.channels;
}


void SVSTiledScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
                                                 const std::vector<int>& channelIndices, int zSliceIndex,
                                                 int tFrameIndex, cv::OutputArray output) {
    if (zSliceIndex != 0 || tFrameIndex != 0) {
        RAISE_RUNTIME_ERROR << "SVSDriver: 3D and 4D images are not supported";
    }
    auto hFile = getFileHandle();
    if (hFile == nullptr) {
        RAISE_RUNTIME_ERROR << "SVSDriver: Invalid file header by raster reading operation";
    }
    double zoomX = static_cast<double>(blockSize.width) / static_cast<double>(blockRect.width);
    double zoomY = static_cast<double>(blockSize.height) / static_cast<double>(blockRect.height);
    double zoom = std::max(zoomX, zoomY);
    const slideio::TiffDirectory& dir = findZoomDirectory(zoom);
    double zoomDirX = static_cast<double>(dir.width) / static_cast<double>(m_directories[0].width);
    double zoomDirY = static_cast<double>(dir.height) / static_cast<double>(m_directories[0].height);
    cv::Rect resizedBlock;
    Tools::scaleRect(blockRect, zoomDirX, zoomDirY, resizedBlock);
    std::vector<int> channels(channelIndices);
    if (channels.empty()) {
        channels.resize(dir.channels);
        std::iota(channels.begin(), channels.end(), 0);
    }
    TileComposer::composeRect(this, channels, resizedBlock, blockSize, output, (void*)&dir);
}

const TiffDirectory& SVSTiledScene::findZoomDirectory(double zoom) const {
    const cv::Rect sceneRect = getRect();
    const double sceneWidth = static_cast<double>(sceneRect.width);
    const auto& directories = m_directories;
    int index = Tools::findZoomLevel(zoom, (int)m_directories.size(), [&directories, sceneWidth](int index) {
        return directories[index].width / sceneWidth;
    });
    return m_directories[index];
}

int SVSTiledScene::getTileCount(void* userData) {
    const TiffDirectory* dir = (const TiffDirectory*)userData;
    int tilesX = (dir->width - 1) / dir->tileWidth + 1;
    int tilesY = (dir->height - 1) / dir->tileHeight + 1;
    return tilesX * tilesY;
}

bool SVSTiledScene::getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) {
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
                             void* userData) {
    const TiffDirectory* dir = static_cast<const TiffDirectory*>(userData);
    bool ret = false;
    try {
        TiffTools::readTile(getFileHandle(), *dir, tileIndex, channelIndices, tileRaster);
        ret = true;
    }
    catch (slideio::RuntimeError&) {
        const cv::Size tileSize = {dir->tileWidth, dir->tileHeight};
        const slideio::DataType dt = dir->dataType;
        tileRaster.create(tileSize, CV_MAKETYPE(slideio::CVTools::toOpencvType(dt), dir->channels));
        tileRaster.setTo(0);
    }

    return ret;
}

void SVSTiledScene::initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices,
                                    cv::OutputArray output) {
    initializeSceneBlock(blockSize, channelIndices, output);
}
