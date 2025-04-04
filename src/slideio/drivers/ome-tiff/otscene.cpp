// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "slideio/drivers/ome-tiff/otscene.hpp"

#include <filesystem>

#include "slideio/imagetools/tifftools.hpp"
#include "slideio/drivers/ome-tiff/ottools.hpp"
#include "slideio/drivers/ome-tiff/otstructs.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include "slideio/imagetools/libtiff.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include <tinyxml2.h>

#include "slideio/base/log.hpp"

using namespace slideio;
using namespace slideio::ometiff;


OTScene::OTScene(const ImageData& imageData){
	m_imageXml = imageData.imageXml;
    m_imageDoc = imageData.doc;
	m_imageId = imageData.imageId;
	m_filePath = imageData.imageFilePath;
    initialize();
}

void OTScene::extractMagnificationFromMetadata() {
    tinyxml2::XMLElement* xmlInstrumentRef = m_imageXml->FirstChildElement("InstrumentRef");
    const char* instrumentRef = nullptr;
    if (xmlInstrumentRef) {
        instrumentRef = xmlInstrumentRef->Attribute("ID");
    }
    tinyxml2::XMLElement* xmlObjectiveRef = m_imageXml->FirstChildElement("ObjectiveSettings");
    const char* objectiveRef = nullptr;
    if (xmlObjectiveRef) {
        objectiveRef = xmlObjectiveRef->Attribute("ID");
    }
    if (instrumentRef != nullptr && objectiveRef != nullptr) {
        tinyxml2::XMLElement* xmlRoot = m_imageDoc->RootElement();
        for (tinyxml2::XMLElement* xmlInstrument = xmlRoot->FirstChildElement("Instrument");
             xmlInstrument != nullptr;
             xmlInstrument = xmlInstrument->NextSiblingElement("Instrument")) {
            const char* id = xmlInstrument->Attribute("ID");
            if (id != nullptr && strcmp(id, instrumentRef) == 0) {
                for (tinyxml2::XMLElement* xmlObjective = xmlInstrument->FirstChildElement("Objective");
                     xmlObjective != nullptr;
                     xmlObjective = xmlObjective->NextSiblingElement("Objective")) {
                    const char* objId = xmlObjective->Attribute("ID");
                    if (objId != nullptr && strcmp(objId, objectiveRef) == 0) {
                        xmlObjective->QueryDoubleAttribute("NominalMagnification", &m_magnification);
                        break;
                    }
                }
                break;
            }
        }
    }
}

void OTScene::extractTiffData(tinyxml2::XMLElement* pixels) {
    std::string directoryPath = std::filesystem::path(m_filePath).parent_path().string();
    std::string fileName = std::filesystem::path(m_filePath).filename().string();

    for (tinyxml2::XMLElement* xmlTiffData = pixels->FirstChildElement("TiffData");
         xmlTiffData != nullptr;
         xmlTiffData = xmlTiffData->NextSiblingElement("TiffData")) {
        TiffData tiffData;
        tiffData.firstChannel = xmlTiffData->IntAttribute("FirstC", 0);
        tiffData.firstZSlice = xmlTiffData->IntAttribute("FirstZ", 0);
        tiffData.firstTFrame = xmlTiffData->IntAttribute("FirstT", 0);
        tiffData.IFD = xmlTiffData->IntAttribute("IFD", 0);
        tiffData.planeCount = xmlTiffData->IntAttribute("PlaneCount", 0);
        tinyxml2::XMLElement* uuid = xmlTiffData->FirstChildElement("UUID");
        if (!uuid) {
            SLIDEIO_LOG(WARNING) << "OTScene: missing required UUID element in the xml metadata";
        }
        const char* fileNameAttr = uuid->Attribute("FileName");
        tiffData.filePath = (fileNameAttr == nullptr || *fileNameAttr == '\0') ? m_filePath : std::filesystem::path(directoryPath).append(fileNameAttr).string();
        if (tiffData.firstChannel >= 0 && tiffData.firstZSlice >= 0 && tiffData.firstTFrame >= 0 && tiffData.IFD >= 0 && tiffData.planeCount > 0) {
            m_tiffData.push_back(tiffData);
        }
        else {
            SLIDEIO_LOG(WARNING) << "Invalid TiffData element in the xml metadata: "
                "FirstC: " << tiffData.firstChannel
                << " FirstZ: " << tiffData.firstZSlice
                << " FirstT: " << tiffData.firstTFrame
                << " IFD: " << tiffData.IFD
                << " PlaneCount: " << tiffData.planeCount;
        }
    }

    std::map<std::string, libtiff::TIFF*> tiffFiles;
    for (TiffData& tiffData : m_tiffData) {
        auto it = tiffFiles.find(tiffData.filePath);
        libtiff::TIFF* tiff = nullptr;
        if (it == tiffFiles.end()) {
            tiff = TiffTools::openTiffFile(tiffData.filePath);
            if (!tiff) {
                SLIDEIO_LOG(WARNING) << "OTScene: cannot open file " << tiffData.filePath << " with libtiff";
                continue;
            }
            tiffFiles[tiffData.filePath] = tiff;
        }
        else {
            tiff = it->second;
        }
        if (tiff == nullptr) {
            SLIDEIO_LOG(WARNING) << "OTScene: cannot open file " << tiffData.filePath << " with libtiff";
            continue;
        }
        tiffData.tiff = tiff;
        std::vector<TiffDirectory> dirs;
        //TiffTools::scanFile(tiff, dirs);
        TiffTools::scanTiffDir(tiff, tiffData.IFD, 0, tiffData.tiffDirectory);
    }
    auto ret = std::remove_if(m_tiffData.begin(), m_tiffData.end(),
        [](const TiffData& tiffData) { return tiffData.tiff == nullptr; });
    m_tiffData.erase(ret, m_tiffData.end());
    if (m_tiffData.empty()) {
        RAISE_RUNTIME_ERROR << "OTScene: no valid TiffData elements in the xml metadata";
    }
}

void OTScene::extractImageIndex() {
	const std::string prefix = "Image:";
    size_t pos = m_imageId.find(prefix);
    if (pos != std::string::npos) {
        pos += prefix.length();
        std::string indexStr = m_imageId.substr(pos);
        try {
            m_imageIndex = std::stoi(indexStr);
        }
        catch (const std::invalid_argument& e) {
            SLIDEIO_LOG(WARNING) << "OTScene: invalid digital index in m_imageId: " << m_imageId;
        }
        catch (const std::out_of_range& e) {
            SLIDEIO_LOG(WARNING) << "OTScene: digital index out of range in m_imageId: " << m_imageId;
        }
    }
}

LevelInfo OTScene::extractLevelInfo(const TiffDirectory& dir, int index) const {
    LevelInfo level;
    level.setLevel(index);
    double coefX = static_cast<double>(dir.width) / static_cast<double>(m_imageSize.width);
    double coefY = static_cast<double>(dir.height) / static_cast<double>(m_imageSize.height);
    double scale = std::max(coefX, coefY);
    level.setMagnification(m_magnification * scale);
    level.setSize(Size(dir.width, dir.height));
    level.setScale(scale);
    level.setTileSize(Size(dir.tileWidth, dir.tileHeight));
    return level;
}

void OTScene::extractImagePyramids() {
	if (m_tiffData.size() == 1) {
        const TiffDirectory& directory = m_tiffData.front().tiffDirectory;
        const int levels = static_cast<int>(directory.subdirectories.size()) + 1;
		m_levels.reserve(levels);
		m_levels.push_back(extractLevelInfo(directory,0));
		for (int i = 1; i < levels; ++i) {
			const auto& subDir = directory.subdirectories[i - 1];
            m_levels.push_back(extractLevelInfo(subDir, i));
        }
	}
    else {
        const TiffDirectory& directory = m_tiffData.front().tiffDirectory;
        m_levels.reserve(1);
        m_levels.push_back(extractLevelInfo(directory, 0));
    }
}

void OTScene::initialize() {
	if (m_imageXml == nullptr || m_imageDoc == nullptr) {
		RAISE_RUNTIME_ERROR << "OTScene: Image xml is not set";
	}
	const char* name = m_imageXml->Attribute("Name");
	m_imageName = (name == nullptr || *name == '\0') ? m_imageId : name;

	tinyxml2::XMLElement* pixels = m_imageXml->FirstChildElement("Pixels");
	if (!pixels) {
		RAISE_RUNTIME_ERROR << "OTScene; missing required Pixels element in the xml metadata";
	}
	const char* dimOrder = pixels->Attribute("DimensionOrder");
	if (!dimOrder) {
		RAISE_RUNTIME_ERROR << "OTScene: missing required DimensionOrder attribute in the xml metadata";
	}
	m_dimensionOrder = dimOrder;
	const char* pixelType = pixels->Attribute("Type");
	if (!pixelType) {
		RAISE_RUNTIME_ERROR << "OTScene: missing required Type attribute in the xml metadata";
	}
	m_dataType = OTTools::stringToDataType(pixelType);
	if (m_dataType == DataType::DT_Unknown) {
		RAISE_RUNTIME_ERROR << "OTScene: unsupported pixel type: " << pixelType;
	}
	m_numChannels = pixels->IntAttribute("SizeC", 0);
	if (m_numChannels <= 0) {
		RAISE_RUNTIME_ERROR << "OTScene: invalid number of channels: " << m_numChannels;
	}
	m_numZSlices = pixels->IntAttribute("SizeZ", 0);
	if (m_numZSlices <= 0) {
		RAISE_RUNTIME_ERROR << "OTScene: invalid number of z-slices: " << m_numZSlices;
	}
	m_numTFrames = pixels->IntAttribute("SizeT", 0);
	if (m_numTFrames <= 0) {
		RAISE_RUNTIME_ERROR << "OTScene: invalid number of time frames: " << m_numTFrames;
	}
	m_imageSize.width = pixels->IntAttribute("SizeX", 0);
	m_imageSize.height = pixels->IntAttribute("SizeY", 0);
	if (m_imageSize.width <= 0 || m_imageSize.height <= 0) {
		RAISE_RUNTIME_ERROR << "OTScene: invalid image size: " << m_imageSize.width << "x" << m_imageSize.height;
	}
	m_bigEndian = pixels->BoolAttribute("BigEndian", false);
	if (m_bigEndian) {
		RAISE_RUNTIME_ERROR << "OTScene: big endian data is not supported";
	}
	extractImageIndex();
    extractTiffData(pixels);
    extractMagnificationFromMetadata();

    int width = m_imageSize.width;
    int height = m_imageSize.height;
    auto mainDir = std::find_if(m_tiffData.begin(), m_tiffData.end(),
        [width, height](const TiffData& data) {return data.tiffDirectory.width == width && data.tiffDirectory.height == height; });
    if (mainDir == m_tiffData.end()) {
        RAISE_RUNTIME_ERROR << "OTScene: main directory not found";
    }
    const TiffDirectory& dir = mainDir->tiffDirectory;
    m_compression = dir.slideioCompression;
    m_resolution = dir.res;

    extractImagePyramids();
}

std::string OTScene::getName() const {
	return m_imageName;
}

slideio::DataType OTScene::getChannelDataType(int channel) const {
	return m_dataType;
}

Resolution OTScene::getResolution() const {
	return m_resolution;
}

double OTScene::getMagnification() const {
	return m_magnification;
}

Compression OTScene::getCompression() const {
	return m_compression;
}

int OTScene::getNumZSlices() const {
    return m_numZSlices;
}

int OTScene::getNumTFrames() const {
    return m_numTFrames;
}

//void OTScene::initialize() {
//    const auto& directory = m_directories[0];
//    const auto& description = directory.description;
//    if (!description.empty()) {
//        tinyxml2::XMLDocument doc;
//        tinyxml2::XMLError error = doc.Parse(description.c_str(), description.size());
//        if (error != tinyxml2::XML_SUCCESS) {
//            RAISE_RUNTIME_ERROR << "OTImageDriver: Error parsing image description xml: " << static_cast<int>(error);
//        }
//        tinyxml2::XMLElement* root = doc.RootElement();
//        if (!root) {
//            RAISE_RUNTIME_ERROR << "OTImageDriver: Error parsing image description xml: root element is null";
//        }
//        // Extract magnification
//        double magnification = -1.;
//        auto xmlScanResolution = root->FirstChildElement("ScanResolution");
//        if (xmlScanResolution) {
//            auto xmlMagnification = xmlScanResolution->FirstChildElement("Magnification");
//            if (xmlMagnification) {
//                magnification = xmlMagnification->DoubleText();
//            }
//        }
//        if (magnification < 0) {
//            auto xmlObjective = root->FirstChildElement("Objective");
//            if (xmlObjective) {
//                magnification = xmlObjective->DoubleText();
//            }
//        }
//        if (magnification > 0) {
//            m_magnification = magnification;
//        }
//        // Extract name
//        auto xmlName = root->FirstChildElement("SlideID");
//        if (xmlName) {
//            m_name = xmlName->GetText();
//        }
//        // Extract isUnmixed
//        auto xmlUnmixed = root->FirstChildElement("IsUnmixedComponent");
//        if (xmlUnmixed) {
//            m_isUnmixed = xmlUnmixed->BoolText();
//        }
//    }
//
//    auto& dir = m_directories[0];
//    m_dataType = dir.dataType;
//    m_resolution = dir.res;
//
//    if (m_dataType == DataType::DT_None || m_dataType == DataType::DT_Unknown) {
//        switch (dir.bitsPerSample) {
//        case 8:
//            m_dataType = dir.dataType = DataType::DT_Byte;
//            break;
//        case 16:
//            m_dataType = dir.dataType = DataType::DT_UInt16;
//            break;
//        default:
//            m_dataType = DataType::DT_Unknown;
//        }
//    }
//
//    if (!m_directories.empty()) {
//        const auto& dir = m_directories.front();
//        m_compression = dir.slideioCompression;
//        if (m_compression == Compression::Unknown &&
//            (dir.compression == 33003 || dir.compression == 3305)) {
//            m_compression = Compression::Jpeg2000;
//        }
//        const int fullResolutionWidth = m_directories[0].width;
//        const int fullResolutionHeight = m_directories[0].height;
//
//        int numFullResolutions = 0;
//        int lastWidth = 0;
//        int index = 0;
//        for (const auto& dir : m_directories) {
//            if (dir.width == fullResolutionWidth && dir.height == fullResolutionHeight) {
//                numFullResolutions++;
//            }
//            if (lastWidth != dir.width && dir.width > 0 && dir.height > 0) {
//                lastWidth = dir.width;
//                m_zoomDirectoryIndices.push_back(index);
//            }
//            ++index;
//        }
//
//        m_numChannels = numFullResolutions * m_directories.front().channels;
//
//        const int numLevels = static_cast<int>(m_zoomDirectoryIndices.size());
//        m_levels.resize(numLevels);
//        for (int lv = 0; lv < numLevels; ++lv) {
//            int directoryIndex = m_zoomDirectoryIndices[lv];
//            const TiffDirectory& directory = m_directories[directoryIndex];
//            LevelInfo& level = m_levels[lv];
//            const double scale = static_cast<double>(directory.width) / static_cast<double>(fullResolutionWidth);
//            level.setLevel(lv);
//            level.setScale(scale);
//            level.setSize({directory.width, directory.height});
//            level.setTileSize({directory.tileWidth, directory.tileHeight});
//            level.setMagnification(m_magnification * scale);
//        }
//    }
//    initializeChannelNames();
//}
//
//void OTScene::initializeChannelNames() {
//    const auto& dir0 = m_directories.front();
//    if (dir0.channels == 1) {
//        for (int channel = 0; channel < m_numChannels; ++channel) {
//            const auto& dir = m_directories[channel];
//            std::string name;
//            const auto& description = dir.description;
//            tinyxml2::XMLDocument doc;
//            tinyxml2::XMLError error = doc.Parse(description.c_str(), description.size());
//            if (error != tinyxml2::XML_SUCCESS) {
//                RAISE_RUNTIME_ERROR << "OTImageDriver: Error parsing image description xml: " << static_cast<int>(
//                    error);
//            }
//            tinyxml2::XMLElement* root = doc.RootElement();
//            if (!root) {
//                RAISE_RUNTIME_ERROR << "OTImageDriver: Error parsing image description xml: root element is null";
//            }
//            auto xmlName = root->FirstChildElement("Name");
//            if (xmlName) {
//                name = xmlName->GetText();
//            }
//            m_channelNames.push_back(name);
//        }
//    }
//}
//

cv::Rect OTScene::getRect() const {
	return { cv::Point(0,0), m_imageSize };
}

int OTScene::getNumChannels() const {
    return m_numChannels;
}


//void OTScene::readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
//                                               const std::vector<int>& channelIndices, cv::OutputArray output) {
//    auto hFile = getFileHandle();
//    if (hFile == nullptr)
//        throw std::runtime_error("OMETIFFDriver: Invalid file header by raster reading operation");
//    double zoomX = static_cast<double>(blockSize.width) / static_cast<double>(blockRect.width);
//    double zoomY = static_cast<double>(blockSize.height) / static_cast<double>(blockRect.height);
//    double zoom = std::max(zoomX, zoomY);
//    const int zoomIndex = findZoomLevel(zoom);
//    int dirIndex = m_zoomDirectoryIndices[zoomIndex];
//    const TiffDirectory& dir = m_directories[dirIndex];
//    double zoomDirX = static_cast<double>(dir.width) / static_cast<double>(m_directories[0].width);
//    double zoomDirY = static_cast<double>(dir.height) / static_cast<double>(m_directories[0].height);
//    cv::Rect resizedBlock;
//    Tools::scaleRect(blockRect, zoomDirX, zoomDirY, resizedBlock);
//    TileComposer::composeRect(this, channelIndices, resizedBlock, blockSize, output, (void*)&zoomIndex);
//}
//
//int OTScene::findZoomLevel(double zoom) const {
//    const cv::Rect sceneRect = getRect();
//    const double sceneWidth = static_cast<double>(sceneRect.width);
//    const auto& directories = m_directories;
//    const auto& zoomIndices = m_zoomDirectoryIndices;
//    int index = Tools::findZoomLevel(zoom, (int)m_zoomDirectoryIndices.size(),
//                                     [&directories, &zoomIndices, sceneWidth](int index) {
//                                         int directoryIndex = zoomIndices[index];
//                                         return directories[directoryIndex].width / sceneWidth;
//                                     });
//    return index;
//}

int OTScene::getTileCount(void* userData) {
    //const int level = *(static_cast<int*>(userData));
    //const int dirIndex = m_zoomDirectoryIndices[level];
    //const TiffDirectory& dir = m_directories[dirIndex];
    //if (dir.tiled) {
    //    int tilesX = (dir.width - 1) / dir.tileWidth + 1;
    //    int tilesY = (dir.height - 1) / dir.tileHeight + 1;
    //    return tilesX * tilesY;
    //}
    return 1;
}

bool OTScene::getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) {
    const int tileCount = getTileCount(userData);
    //if (tileIndex >= tileCount) {
    //    RAISE_RUNTIME_ERROR << "OMETIFF driver: invalid tile index: " << tileIndex << " of " << tileCount;
    //}
    //const int level = *(static_cast<int*>(userData));
    //const int dirIndex = m_zoomDirectoryIndices[level];
    //const TiffDirectory& dir = m_directories[dirIndex];
    //if (dir.tiled) {
    //    const int tilesX = (dir.width - 1) / dir.tileWidth + 1;
    //    const int tilesY = (dir.height - 1) / dir.tileHeight + 1;
    //    const int tileY = tileIndex / tilesX;
    //    const int tileX = tileIndex % tilesX;
    //    tileRect.x = tileX * dir.tileWidth;
    //    tileRect.y = tileY * dir.tileHeight;
    //    tileRect.width = dir.tileWidth;
    //    tileRect.height = dir.tileHeight;
    //}
    //else {
    //    tileRect = {0, 0, dir.width, dir.height};
    //}
    return false;
}

bool OTScene::readTiffTile(int tileIndex, const TiffDirectory& dir,
                                          const std::vector<int>& channelIndices, cv::OutputArray tileRaster) {
    bool ret = false;
//    try {
//        if (isBrightField()) {
//            TiffTools::readTile(getFileHandle(), dir, tileIndex, channelIndices, tileRaster);
//            ret = true;
//        }
//        else if (channelIndices.size() == 1) {
//            TiffTools::readTile(getFileHandle(), dir, tileIndex, {0}, tileRaster);
//            ret = true;
//        }
//        else {
//            std::vector<cv::Mat> channelRasters;
//            std::vector<int> channels = Tools::completeChannelList(channelIndices, getNumChannels());
//            for (const auto& channelIndex : channels) {
//                cv::Mat channelRaster;
//                TiffTools::readTile(getFileHandle(), dir, tileIndex, {0}, channelRaster);
//                channelRasters.push_back(channelRaster);
//            }
//            cv::merge(channelRasters, tileRaster);
//            ret = true;
//        }
//    }
//    catch (std::runtime_error&) {
//        const cv::Size tileSize = {dir.tileWidth, dir.tileHeight};
//        const slideio::DataType dt = dir.dataType;
//        tileRaster.create(tileSize, CV_MAKETYPE(slideio::CVTools::toOpencvType(dt), dir.channels));
//        tileRaster.setTo(0);
//    }
//
    return ret;
}

bool OTScene::readTiffDirectory(const TiffDirectory& dir, const std::vector<int>& channelIndices,
                                               cv::OutputArray wholeDirRaster) {
//    cv::Mat dirRaster;
//    TiffTools::readStripedDir(getFileHandle(), dir, dirRaster);
//    Tools::extractChannels(dirRaster, channelIndices, wholeDirRaster);
    return true;
}

void OTScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output) {
}

std::string OTScene::getFilePath() const {
	return m_filePath;
}

bool OTScene::readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
                       void* userData) {
    const int tileCount = getTileCount(userData);
    if (tileIndex >= tileCount) {
        RAISE_RUNTIME_ERROR << "OMETIFF driver: invalid tile index: " << tileIndex << " of " << tileCount;
    }

//    const int level = *(static_cast<int*>(userData));
//    bool ret = false;
//    const int dirIndex = m_zoomDirectoryIndices[level];
//    const TiffDirectory& dir = m_directories[dirIndex];
//    if (dir.tiled) {
//        return readTiffTile(tileIndex, dir, channelIndices, tileRaster);
//    }
//    if (dir.channels == getNumChannels()) {
//        return readTiffDirectory(dir, channelIndices, tileRaster);
//    }
//    if (dir.channels == 1) {
//        auto channels = Tools::completeChannelList(channelIndices, getNumChannels());
//        if (channels.size() == 1) {
//            const TiffDirectory newDir = m_directories.at(dir.dirIndex + channels[0]);
//            return readTiffDirectory(newDir, {0}, tileRaster);
//        }
//        std::vector<cv::Mat> channelRasters;
//        for (const auto& channelIndex : channels) {
//            cv::Mat channelRaster;
//            const TiffDirectory newDir = m_directories.at(dir.dirIndex + channels[0]);
//            TiffTools::readRegularStripedDir(getFileHandle(), newDir, channelRaster);
//            channelRasters.push_back(channelRaster);
//        }
//        cv::merge(channelRasters, tileRaster);
//        return true;
//    }
//    else {
//        RAISE_RUNTIME_ERROR << "OMETIFF driver: Unexpected number of channels in the directory: "
//            << dir.channels << ". Expected: 1 or " << getNumChannels() << ".";
//    }
	return false;
}

void OTScene::initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices,
                                    cv::OutputArray output) {
    initializeSceneBlock(blockSize, channelIndices, output);
}

std::string OTScene::getChannelName(int channel) const {
    return m_channelNames.empty() ? "" : m_channelNames[channel];
}

//bool OTScene::isBrightField() const {
//    return m_directories.front().channels == m_numChannels;
//}
