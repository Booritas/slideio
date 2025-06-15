// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/ome-tiff/otslide.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/drivers/ome-tiff/otscene.hpp"
#include "slideio/drivers/ome-tiff/otstructs.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/base/log.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include <fstream>
#include <tinyxml2.h>
#include <filesystem>


using namespace slideio;
using namespace slideio::ometiff;

const char* THUMBNAIL = "Thumbnail";
const char* MACRO = "Macro";
const char* LABEL = "Label";


OTSlide::OTSlide() {
    m_metadataFormat = MetadataFormat::XML;
}

OTSlide::~OTSlide() {
}

int OTSlide::getNumScenes() const {
    return (int)m_Scenes.size();
}

std::string OTSlide::getFilePath() const {
    return m_filePath;
}

std::shared_ptr<CVScene> OTSlide::getScene(int index) const {
    if (index >= getNumScenes())
        throw std::runtime_error("OMETIFF driver: invalid m_scene index");
    return m_Scenes[index];
}

std::shared_ptr<OTSlide> OTSlide::processMetadata(const std::string& filePath, std::shared_ptr<OTSlide> slide, std::shared_ptr<tinyxml2::XMLDocument> doc) {
    std::list<ImageData> images;
    tinyxml2::XMLElement* root = doc->RootElement();
    if (!root) {
        RAISE_RUNTIME_ERROR << "OTImageDriver: Invalid xml metadata in file: " << filePath;
    }

    for (tinyxml2::XMLElement* imageElem = root->FirstChildElement("Image");
         imageElem != nullptr;
         imageElem = imageElem->NextSiblingElement("Image")) {
        if (const char* id = imageElem->Attribute("ID")) {
            ImageData image = {doc, imageElem, id, filePath};
            images.push_back(image);
        }
    }

    images.sort([](const ImageData& left, const ImageData& right) { return left.imageId < right.imageId; });
    images.unique([](const ImageData& left, const ImageData& right) { return left.imageId == right.imageId; });
    if (images.empty()) {
        RAISE_RUNTIME_ERROR << "OTImageDriver: No image found in the file: " << filePath;
    }

    SLIDEIO_LOG(INFO) << "OTSlide::openFile: Found " << images.size() << " images in file " << filePath;

    slide.reset(new OTSlide);

    for (const ImageData& imageData : images) {
        std::shared_ptr<CVScene> scene = createScene(imageData);
        if (scene) {
            slide->m_Scenes.push_back(scene);
        }
    }
    return slide;
}

std::shared_ptr<OTSlide> OTSlide::openFile(const std::string& filePath) {
    SLIDEIO_LOG(INFO) << "OTSlide::openFile: " << filePath;
    std::shared_ptr<OTSlide> slide;
    std::vector<TiffDirectory> directories;
    libtiff::TIFF* tiff(nullptr);
    tiff = TiffTools::openTiffFile(filePath);
    if (!tiff) {
        SLIDEIO_LOG(WARNING) << "OTSlide::openFile: cannot open file " << filePath << " with libtiff";
        return slide;
    }
    TIFFKeeper keeper(tiff);

    TiffTools::scanFile(tiff, directories);
    auto& dir = directories.front();
    auto description = dir.description;
#if defined(_DEBUG)
     std::string fileName = std::filesystem::path(filePath).stem().string();
     std::string xmlPath = "D:/Temp/" + fileName + ".xml";
     std::ofstream outFile(xmlPath);
     outFile << description;
     outFile.close();
#endif

    if (description.empty()) {
        RAISE_RUNTIME_ERROR << "OTSlide::openFile: cannot find ometiff xml metadata in file " << filePath;
    }

	SLIDEIO_LOG(INFO) << "OTSlide::openFile: Starting parsing xml metadata in file " << filePath;

    std::shared_ptr<tinyxml2::XMLDocument> doc = std::make_shared<tinyxml2::XMLDocument>(new tinyxml2::XMLDocument);

    tinyxml2::XMLError error = doc->Parse(description.c_str(), description.size());
    if (error != tinyxml2::XML_SUCCESS) {
        RAISE_RUNTIME_ERROR << "OTSlide::openFile: error parsing ometiff xml metadata in file " << filePath;
    }

    tinyxml2::XMLElement* root = doc->RootElement();
    if (!root) {
        RAISE_RUNTIME_ERROR << "OTImageDriver: Invalid xml metadata in file: " << filePath;
    }

    tinyxml2::XMLElement* binaryOnlyElem = root->FirstChildElement("BinaryOnly");
    if (binaryOnlyElem != nullptr) {
        const tinyxml2::XMLAttribute* mtd = binaryOnlyElem->FindAttribute("MetadataFile");
		if (mtd != nullptr && mtd->Value() != nullptr) {
			std::string metadataFile = mtd->Value();
			if (!metadataFile.empty()) {
                std::filesystem::path dir = std::filesystem::path(filePath).parent_path();
                std::filesystem::path metadataPath = dir / metadataFile;
				std::ifstream file(metadataPath.c_str());
				if (file.is_open()) {
					std::string metadataContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
					description = metadataContent;
					SLIDEIO_LOG(INFO) << "OTSlide::openFile: Metadata loaded from file: " << metadataFile;
                    tinyxml2::XMLError error = doc->Parse(description.c_str(), description.size());
                    if (error != tinyxml2::XML_SUCCESS) {
                        RAISE_RUNTIME_ERROR << "OTSlide::openFile: error parsing ometiff xml metadata in file " << metadataPath;
                    }
                }
				else {
					RAISE_RUNTIME_ERROR << "OTSlide::openFile: Cannot open metadata file: " << metadataFile;
				}
			}
		}
    }
    slide = processMetadata(filePath, slide, doc);
    slide->m_rawMetadata = description;
    return slide;
}

std::shared_ptr<CVScene> OTSlide::createScene(const ImageData& imageData) {
    std::shared_ptr<CVScene> scene;
    try {
		SLIDEIO_LOG(INFO) << "OTSlide::createScene: Creating scene for image: " << imageData.imageId;
        OTScene* otScene = new OTScene(imageData);
        SLIDEIO_LOG(INFO) << "OTSlide::createScene: Scene " << imageData.imageId << " is successfully created.";
        scene.reset(otScene);
    }
    catch (std::exception& ex) {
        SLIDEIO_LOG(WARNING) << "Error by OME-TIFF scene creation: " << ex.what();
    }
    return scene;
}

std::shared_ptr<CVScene> OTSlide::getAuxImage(const std::string& sceneName) const {
    auto it = m_auxImages.find(sceneName);
    if (it == m_auxImages.end()) {
        RAISE_RUNTIME_ERROR << "The slide does non have auxiliary image " << sceneName;
    }
    return it->second;
}

void OTSlide::log() {
    SLIDEIO_LOG(INFO) << "---OTSlide" << std::endl;
    SLIDEIO_LOG(INFO) << "filePath:" << m_filePath << std::endl;
}
