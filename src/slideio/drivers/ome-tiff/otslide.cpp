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


OTSlide::OTSlide()
{
}

OTSlide::~OTSlide()
{
}

int OTSlide::getNumScenes() const
{
    return (int)m_Scenes.size();
}

std::string OTSlide::getFilePath() const
{
    return m_filePath;
}

std::shared_ptr<CVScene> OTSlide::getScene(int index) const
{
    if(index>=getNumScenes())
        throw std::runtime_error("OMETIFF driver: invalid m_scene index");
    return m_Scenes[index];
}

std::shared_ptr<OTSlide> OTSlide::openFile(const std::string& filePath)
{
    SLIDEIO_LOG(INFO) << "OTSlide::openFile: " << filePath;
    std::shared_ptr<OTSlide> slide;
    std::vector<TiffDirectory> directories;
    libtiff::TIFF* tiff(nullptr);
    tiff = TiffTools::openTiffFile(filePath);
    if(!tiff) {
        SLIDEIO_LOG(WARNING) << "OTSlide::openFile: cannot open file " << filePath << " with libtiff";
        return slide;
    }
    TIFFKeeper keeper(tiff);

    TiffTools::scanFile(tiff, directories);

    std::vector<TiffDirectory> image_dirs;
    std::map<std::string, std::shared_ptr<CVScene>> auxImages;
    std::list<std::string> auxNames;
    std::list<std::string> metadataItems;

    auto& dir = directories.front();
    auto description = dir.description;
#if defined(_DEBUG)
	std::string fileName = std::filesystem::path(filePath).stem().string();
	std::string xmlPath = "D:/Temp/" + fileName + ".xml";
    std::ofstream outFile(xmlPath);
    outFile << description;
    outFile.close();
#endif
    std::list<ImageData> images;

    for (const auto& directory : directories) {
        const auto& description = directory.description;
        if (!description.empty()) {
            std::shared_ptr<tinyxml2::XMLDocument> doc = std::make_shared<tinyxml2::XMLDocument>(new tinyxml2::XMLDocument);

            tinyxml2::XMLError error = doc->Parse(description.c_str(), description.size());
            if (error != tinyxml2::XML_SUCCESS) {
                continue;
            }
            tinyxml2::XMLElement* root = doc->RootElement();
            if(!root) {
                RAISE_RUNTIME_ERROR << "OTImageDriver: Error parsing image description xml: root element is null";
            }
            for (tinyxml2::XMLElement* imageElem = root->FirstChildElement("Image");
                 imageElem != nullptr;
                 imageElem = imageElem->NextSiblingElement("Image")) {
                if (const char* id = imageElem->Attribute("ID")) {
					ImageData image = {doc, imageElem, id, filePath};
					images.push_back(image);
				}
            }
        }
    }
    images.sort([](const ImageData& left, const ImageData& right){ return left.imageId < right.imageId; });
    images.unique([](const ImageData& left, const ImageData& right) {return left.imageId == right.imageId; });
	if (images.empty()) {
		RAISE_RUNTIME_ERROR << "OTImageDriver: No image found in the file: " << filePath;
	}
    slide.reset(new OTSlide);
    for (const ImageData& imageData : images) {
		std::shared_ptr<CVScene> scene = createScene(imageData);
        if(scene) {
            slide->m_Scenes.push_back(scene);
        }
	}
    //std::shared_ptr<CVScene> scene(new OTTiledScene(filePath,keeper.release(),"Image", image_dirs));
    //std::vector<std::shared_ptr<CVScene>> scenes;
    //scenes.push_back(scene);
    //slide->m_Scenes.assign(scenes.begin(), scenes.end());
    //slide->m_filePath = filePath;
    //slide->m_auxImages = auxImages;
    //slide->m_auxNames = auxNames;

    //tinyxml2::XMLDocument xmlMetadata;
    //auto rootMetadata = xmlMetadata.NewElement("Metadata");
    //xmlMetadata.InsertEndChild(rootMetadata);

    //for(const auto& metadataItem: metadataItems) {
    //    tinyxml2::XMLDocument doc;
    //    doc.Parse(metadataItem.c_str());
    //    auto root = doc.RootElement();
    //    rootMetadata->InsertEndChild(root->DeepClone(&xmlMetadata));
    //}
    //tinyxml2::XMLPrinter printer;
    //xmlMetadata.Print(&printer);
    //slide->m_rawMetadata = printer.CStr();
    ////std::ofstream outFile("D:/Temp/output.xml");
    ////outFile << slide->m_rawMetadata;
    ////outFile.close();
    return slide;
}

std::shared_ptr<CVScene> OTSlide::createScene(const ImageData& imageData) {
	std::shared_ptr<CVScene> scene;
    try {
        OTScene* otScene = new OTScene(imageData);
        scene.reset(otScene);
    }
    catch (std::exception& ex) {
		SLIDEIO_LOG(WARNING) << "Error by OME-TIFF scene creation: " << ex.what();
    }
	return scene;
}

std::shared_ptr<CVScene> OTSlide::getAuxImage(const std::string& sceneName) const
{
    auto it = m_auxImages.find(sceneName);
    if(it==m_auxImages.end()) {
        RAISE_RUNTIME_ERROR << "The slide does non have auxiliary image " << sceneName;
    }
    return it->second;
}

void OTSlide::log()
{
    SLIDEIO_LOG(INFO) << "---OTSlide" << std::endl;
    SLIDEIO_LOG(INFO) << "filePath:" << m_filePath << std::endl;
}
