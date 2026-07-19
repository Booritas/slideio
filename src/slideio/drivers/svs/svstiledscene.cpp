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
    if (m_driverId == "SVS") {
        processImageDescriptionSVS();
    }
    else if (m_driverId == "PHTIFF") {
        processImageDescriptionPhTiff();
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
            "SVSTiledScene::initialize: directories are empty, scene initialization may be incorrect";
    }
    const std::string& description = m_directories.front().description;

    tinyxml2::XMLDocument doc;
    if (doc.Parse(description.c_str(), description.size()) != tinyxml2::XML_SUCCESS) {
        RAISE_RUNTIME_ERROR << "SVSTiledScene::openFile: error parsing philips xml metadata.";
    }
    const tinyxml2::XMLElement* root = doc.RootElement();
    if (!root) {
        RAISE_RUNTIME_ERROR << "SVSTiledScene: Invalid xml metadata.";
    }

    constexpr const char* SCANNED_IMAGES = "PIM_DP_SCANNED_IMAGES";
    const size_t nameLen = strlen(SCANNED_IMAGES);

    // Walk PIM_DP_SCANNED_IMAGES > DPScannedImage > DICOM_PIXEL_SPACING and read the
    // pixel spacing. The first match fully describes the resolution, so stop once found.
    for (const tinyxml2::XMLElement* attribute = root->FirstChildElement("Attribute");
         attribute != nullptr;
         attribute = attribute->NextSiblingElement("Attribute")) {
        const char* name = attribute->Attribute("Name");
        if (name == nullptr || strncmp(name, SCANNED_IMAGES, nameLen) != 0) {
            continue;
        }
        const tinyxml2::XMLElement* array = attribute->FirstChildElement("Array");
        if (array == nullptr) {
            continue;
        }
        for (const tinyxml2::XMLElement* dataObject = array->FirstChildElement("DataObject");
             dataObject != nullptr;
             dataObject = dataObject->NextSiblingElement("DataObject")) {
            const char* objType = dataObject->Attribute("ObjectType");
            if (objType == nullptr || strcmp(objType, "DPScannedImage") != 0) {
                continue;
            }
            for (const tinyxml2::XMLElement* objAttribute = dataObject->FirstChildElement("Attribute");
                 objAttribute != nullptr;
                 objAttribute = objAttribute->NextSiblingElement("Attribute")) {
                const char* objName = objAttribute->Attribute("Name");
                if (objName == nullptr || strcmp(objName, "DICOM_PIXEL_SPACING") != 0) {
                    continue;
                }
                const char* pmsvr = objAttribute->Attribute("PMSVR");
                if (pmsvr == nullptr || strcmp(pmsvr, "IDoubleArray") != 0) {
                    SLIDEIO_LOG(WARNING) << "Not supported type of DICOM_PIXEL_SPACING attribute: "
                                         << (pmsvr ? pmsvr : "(null)");
                    return;
                }
                if (const char* value = objAttribute->GetText()) {
                    // The value is a pair of quoted doubles, e.g. "0.000226891" "0.000226907".
                    // Strip the quotes and read the two numbers.
                    std::string spacing(value);
                    std::replace(spacing.begin(), spacing.end(), '"', ' ');
                    std::istringstream stream(spacing);
                    double x = 0., y = 0.;
                    if (stream >> x >> y) {
                        m_resolution = {x, y};
                    }
                }
                return;
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
