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
#include "slideio/imagetools/uridispatcher.hpp"
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

std::shared_ptr<OTSlide> OTSlide::createSlide(const std::string& filePath, const std::string& driverId, std::shared_ptr<tinyxml2::XMLDocument> doc, std::shared_ptr<RandomAccessStream> stream) {
    std::list<ImageData> images;
    tinyxml2::XMLElement* root = doc->RootElement();
    if (!root) {
        RAISE_RUNTIME_ERROR << "OTImageDriver: Invalid xml metadata in file: " << filePath;
    }

    for (tinyxml2::XMLElement* imageElem = root->FirstChildElement("Image");
         imageElem != nullptr;
         imageElem = imageElem->NextSiblingElement("Image")) {
        if (const char* id = imageElem->Attribute("ID")) {
            ImageData image = {doc, imageElem, id, filePath, stream};
            images.push_back(image);
        }
    }

    images.sort([](const ImageData& left, const ImageData& right) { return left.imageId < right.imageId; });
    images.unique([](const ImageData& left, const ImageData& right) { return left.imageId == right.imageId; });
    if (images.empty()) {
        RAISE_RUNTIME_ERROR << "OTImageDriver: No image found in the file: " << filePath;
    }

    SLIDEIO_LOG(INFO) << "OTSlide::openFile: Found " << images.size() << " images in file " << filePath;

	std::shared_ptr<OTSlide> slide(new OTSlide);
	slide->setDriverId(driverId);

    for (const ImageData& imageData : images) {
        std::shared_ptr<CVScene> scene = createScene(imageData, static_cast<int>(slide->m_Scenes.size()), slide->getDriverId());
        if (scene) {
            slide->m_Scenes.push_back(scene);
        }
    }
    slide->m_stream = stream;
    return slide;
}

std::shared_ptr<OTSlide> OTSlide::openFile(const std::string& filePath, const std::string& driverId) {
    SLIDEIO_LOG(INFO) << "OTSlide::openFile (path): " << filePath;
    libtiff::TIFF* tiff = TiffTools::openTiffFile(filePath);
    if (!tiff) {
        SLIDEIO_LOG(WARNING) << "OTSlide::openFile: cannot open file " << filePath << " with libtiff";
        return std::shared_ptr<OTSlide>();
    }
    return openFile(tiff, filePath, driverId, nullptr);
}

std::shared_ptr<OTSlide> OTSlide::openFile(std::shared_ptr<RandomAccessStream> stream, const std::string& driverId) {
    const std::string identifier = stream ? stream->uri() : std::string();
    SLIDEIO_LOG(INFO) << "OTSlide::openFile (stream): " << identifier;
    libtiff::TIFF* tiff = TiffTools::openTiffFile(stream);
    if (!tiff) {
        SLIDEIO_LOG(WARNING) << "OTSlide::openFile: cannot open stream " << identifier << " with libtiff";
        return std::shared_ptr<OTSlide>();
    }
    return openFile(tiff, identifier, driverId, stream);
}

std::shared_ptr<OTSlide> OTSlide::openFile(libtiff::TIFF* tiff,
                                           const std::string& filePath,
                                           const std::string& driverId,
                                           std::shared_ptr<RandomAccessStream> stream) {
    std::shared_ptr<OTSlide> slide;
    std::vector<TiffDirectory> directories;
    TIFFKeeper keeper(tiff);

    TiffTools::scanFile(tiff, directories);
    auto& dir = directories.front();
    auto description = dir.description;
#if defined(_DEBUG)
     // std::string fileName = std::filesystem::path(filePath).stem().string();
     // std::string xmlPath = "D:/Temp/" + fileName + ".xml";
     // std::ofstream outFile(xmlPath);
     // outFile << description;
     // outFile.close();
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
                std::string metadataContent;
                std::string metadataLocation;
                if (stream) {
                    // Resolve the companion XML next to the originating URI and read
                    // it (small) via a sibling stream rather than the local filesystem.
                    const std::string metadataUri = slideio::siblingUri(filePath, metadataFile);
                    metadataLocation = metadataUri;
                    std::shared_ptr<RandomAccessStream> mtdStream = slideio::createStream(metadataUri);
                    if (!mtdStream) {
                        RAISE_RUNTIME_ERROR << "OTSlide::openFile: Cannot open metadata stream: " << metadataUri;
                    }
                    const uint64_t mtdSize = mtdStream->size();
                    metadataContent.resize(static_cast<size_t>(mtdSize));
                    if (mtdSize > 0) {
                        const size_t read = mtdStream->read(0, static_cast<size_t>(mtdSize), &metadataContent[0]);
                        metadataContent.resize(read);
                    }
                    SLIDEIO_LOG(INFO) << "OTSlide::openFile: Metadata loaded from stream: " << metadataUri;
                }
                else {
                    std::filesystem::path dir = std::filesystem::path(filePath).parent_path();
                    std::filesystem::path metadataPath = dir / metadataFile;
                    metadataLocation = metadataPath.string();
                    std::ifstream file(metadataPath.c_str());
                    if (!file.is_open()) {
                        RAISE_RUNTIME_ERROR << "OTSlide::openFile: Cannot open metadata file: " << metadataFile;
                    }
                    metadataContent.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                    SLIDEIO_LOG(INFO) << "OTSlide::openFile: Metadata loaded from file: " << metadataFile;
                }
                description = metadataContent;
                tinyxml2::XMLError mtdError = doc->Parse(description.c_str(), description.size());
                if (mtdError != tinyxml2::XML_SUCCESS) {
                    RAISE_RUNTIME_ERROR << "OTSlide::openFile: error parsing ometiff xml metadata in file " << metadataLocation;
                }
			}
		}
    }
    slide = createSlide(filePath, driverId, doc, stream);
    slide->m_rawMetadata = description;
    return slide;
}

std::shared_ptr<CVScene> OTSlide::createScene(const ImageData& imageData, int sceneIndex, const std::string& driverId) {
    std::shared_ptr<CVScene> scene;
    try {
		SLIDEIO_LOG(INFO) << "OTSlide::createScene: Creating scene for image: " << imageData.imageId;
        OTScene* otScene = new OTScene(imageData, sceneIndex, driverId);
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
