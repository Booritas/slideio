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
#include "slideio/drivers/ome-tiff/tiffdata.hpp"
#include <tinyxml2.h>

#include "slideio/base/log.hpp"

using namespace slideio;
using namespace slideio::ometiff;

struct BlockInfo
{
    LevelInfo* levelInfo = nullptr;
    int zSliceIndex = -1;
    int tFrameIndex = -1;
};


OTScene::OTScene(const ImageData& imageData) {
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
        try {
            TiffData tiffData;
            tiffData.init(directoryPath, &m_files, &m_dimensions, xmlTiffData);
            m_tiffData.push_back(tiffData);
        }
        catch (std::exception& e) {
			SLIDEIO_LOG(WARNING) << "OTScene: failed to extract TiffData element from xml metadata: " << e.what();
        }
    }
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
        catch (const std::invalid_argument&) {
            SLIDEIO_LOG(WARNING) << "OTScene: invalid digital index in m_imageId: " << m_imageId;
        }
        catch (const std::out_of_range&) {
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
    if (!m_tiffData.empty()) {
        const TiffDirectory& directory = m_tiffData.front().getTiffDirectory(0);
        const int levels = static_cast<int>(directory.subdirectories.size()) + 1;
        m_levels.reserve(levels);
        m_levels.push_back(extractLevelInfo(directory, 0));
        for (int i = 1; i < levels; ++i) {
            const auto& subDir = directory.subdirectories[i - 1];
            m_levels.push_back(extractLevelInfo(subDir, i));
        }
    }
}

void OTScene::initializeDimensions() {
	m_dimensions.init(m_dimensionOrder, m_numChannels, m_numZSlices, m_numTFrames, m_samplesPerPixel);
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
	m_samplesPerPixel = pixels->IntAttribute("SamplesPerPixel", 1);

    initializeDimensions();
    extractImageIndex();
    extractTiffData(pixels);
    extractMagnificationFromMetadata();


    int width = m_imageSize.width;
    int height = m_imageSize.height;
    auto dir = m_tiffData.front().getTiffDirectory(0);
    m_compression = dir.slideioCompression;
    m_resolution = dir.res;

    extractImagePyramids();
}


std::pair<const TiffData*, int> OTScene::findTiffData(int channel, int slice, int frame) const {
	std::pair<const TiffData*, int> result = { nullptr, -1 };
	for (const auto& tiffData : m_tiffData) {
		if (tiffData.isInRange(channel, slice, frame)) {
   //         std::list<std::pair<std::string, int>> listCoords = {
   //         { DimC, tiffData.getChannelRange().start },
   //         { DimZ, tiffData.getZSliceRange().start },
   //         { DimT, tiffData.getTFrameRange().start }
   //         };
			//auto coords = m_dimensions.createCoordinates(listCoords);
			//int channelIndex = m_dimensions.getDimensionIndex(DimC);
			//int zIndex = m_dimensions.getDimensionIndex(DimZ);
			//int tIndex = m_dimensions.getDimensionIndex(DimT);
			//for (int plane = 0; plane < tiffData.getPlaneCount(); ++plane) {
   //             if (coords[channelIndex] == channel &&
   //                 coords[zIndex] == slice &&
   //                 coords[tIndex] == frame) {
			//		result.first = &tiffData;
			//		result.second = plane;
			//		return result;
   //             }
			//	m_dimensions.incrementCoordinates(coords);
			//}
		}
	}
	return result;
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


cv::Rect OTScene::getRect() const {
    return {cv::Point(0, 0), m_imageSize};
}

int OTScene::findZoomLevel(double zoom) const {
    const cv::Rect sceneRect = getRect();
    const double sceneWidth = static_cast<double>(sceneRect.width);
    const auto& levels = m_levels;
    int index = Tools::findZoomLevel(zoom, (int)m_levels.size(),
                                     [&levels, sceneWidth](int index) {
                                         const auto& level = levels[index];
                                         return 1. / level.getScale();
                                     });
    return index;
}

int OTScene::getNumChannels() const {
    return m_numChannels;
}


int OTScene::getTileCount(void* userData) {
    const BlockInfo* blockInfo = static_cast<const BlockInfo*>(userData);
    return blockInfo->levelInfo->getTileCount();
}

bool OTScene::getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) {
    const int tileCount = getTileCount(userData);
    if (tileIndex >= tileCount) {
        RAISE_RUNTIME_ERROR << "OMETIFF driver: invalid tile index: " << tileIndex << " of " << tileCount;
    }
    const BlockInfo* blockInfo = static_cast<const BlockInfo*>(userData);
    tileRect = blockInfo->levelInfo->getTileRect(tileIndex);
    return true;
}


void OTScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
                                           const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex,
                                           cv::OutputArray output) {
    double zoomX = static_cast<double>(blockSize.width) / static_cast<double>(blockRect.width);
    double zoomY = static_cast<double>(blockSize.height) / static_cast<double>(blockRect.height);
    double zoom = std::max(zoomX, zoomY);
    auto channelIndices = Tools::completeChannelList(componentIndices, m_numChannels);
    const int zoomIndex = findZoomLevel(zoom);
    const LevelInfo& levelInfo = m_levels[zoomIndex];
    double zoomDirX = static_cast<double>(levelInfo.getSize().width) / static_cast<double>(m_imageSize.width);
    double zoomDirY = static_cast<double>(levelInfo.getSize().height) / static_cast<double>(m_imageSize.height);
    cv::Rect resizedBlock;
    Tools::scaleRect(blockRect, zoomDirX, zoomDirY, resizedBlock);
    BlockInfo blockInfo = {(LevelInfo*)&levelInfo, zSliceIndex, tFrameIndex};
    TileComposer::composeRect(this, channelIndices, resizedBlock, blockSize, output, (void*)&blockInfo);
}

std::string OTScene::getFilePath() const {
    return m_filePath;
}

bool OTScene::readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
                       void* userData) {
    const BlockInfo* blockInfo = static_cast<const BlockInfo*>(userData);
	const int zSlice = blockInfo->zSliceIndex;
	const int tFrame = blockInfo->tFrameIndex;
    const int tileCount = getTileCount(userData);
    const int zoomLevel = blockInfo->levelInfo->getLevel();
    if (tileIndex >= tileCount) {
        RAISE_RUNTIME_ERROR << "OMETIFF driver: invalid tile index: " << tileIndex << " of " << tileCount;
    }
	std::vector<cv::Mat> channelRasters(channelIndices.size());
    std::vector<int> channelsToFill(channelIndices);
    for (const auto& tiffData : m_tiffData) {
        tiffData.readTile(channelsToFill, zSlice, tFrame, zoomLevel, tileIndex, channelRasters);
		if (channelsToFill.empty()) {
			break;
		}
    }
    if (!channelsToFill.empty()) {
		SLIDEIO_LOG(WARNING) << "OTScene: not all channels were read from the tile";
        return false;
    }
    return true;
}

void OTScene::initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices,
                              cv::OutputArray output) {
    initializeSceneBlock(blockSize, channelIndices, output);
}

std::string OTScene::getChannelName(int channel) const {
    return m_channelNames.empty() ? "" : m_channelNames[channel];
}
