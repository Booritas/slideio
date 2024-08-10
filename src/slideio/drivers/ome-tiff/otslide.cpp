// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/ome-tiff/otslide.hpp"

#include <fstream>
#include <tinyxml2.h>

#include "slideio/imagetools/imagetools.hpp"
#include "slideio/drivers/ome-tiff/otsmallscene.hpp"
#include "slideio/drivers/ome-tiff/ottiledscene.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/base/base.hpp"

#include <boost/filesystem.hpp>
#include <boost/format.hpp>


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

    for (const auto& directory : directories) {
        const auto& description = directory.description;
        if (!description.empty()) {
            tinyxml2::XMLDocument doc;
            tinyxml2::XMLError error = doc.Parse(description.c_str(), description.size());
            if (error != tinyxml2::XML_SUCCESS) {
                RAISE_RUNTIME_ERROR << "OTImageDriver: Error parsing image description xml: " << static_cast<int>(error);
            }
            tinyxml2::XMLElement* root = doc.RootElement();
            if(!root) {
                RAISE_RUNTIME_ERROR << "OTImageDriver: Error parsing image description xml: root element is null";
            }
            auto xmlImageType= root->FirstChildElement("ImageType");
            if(xmlImageType) {
                std::string name;
                std::string type = xmlImageType->GetText();
                if(type == "FullResolution" || type == "ReducedResolution") {
                    if(type == "FullResolution") {
                        metadataItems.push_back(description);
                    }
                    image_dirs.push_back(directory);
                } else if(type == "Thumbnail" || type == "Overview" || type == "Label") {
                    std::shared_ptr<CVScene> scene(new OTSmallScene(filePath, type, directory, true));
                    auxImages[type] = scene;
                    auxNames.emplace_back(type);
                }
            }
        }
    }

    std::shared_ptr<CVScene> scene(new OTTiledScene(filePath,keeper.release(),"Image", image_dirs));
    std::vector<std::shared_ptr<CVScene>> scenes;
    scenes.push_back(scene);
    slide.reset(new OTSlide);
    slide->m_Scenes.assign(scenes.begin(), scenes.end());
    slide->m_filePath = filePath;
    slide->m_auxImages = auxImages;
    slide->m_auxNames = auxNames;

    tinyxml2::XMLDocument xmlMetadata;
    auto rootMetadata = xmlMetadata.NewElement("Metadata");
    xmlMetadata.InsertEndChild(rootMetadata);

    for(const auto& metadataItem: metadataItems) {
        tinyxml2::XMLDocument doc;
        doc.Parse(metadataItem.c_str());
        auto root = doc.RootElement();
        rootMetadata->InsertEndChild(root->DeepClone(&xmlMetadata));
    }
    tinyxml2::XMLPrinter printer;
    xmlMetadata.Print(&printer);
    slide->m_rawMetadata = printer.CStr();
    //std::ofstream outFile("D:/Temp/output.xml");
    //outFile << slide->m_rawMetadata;
    //outFile.close();
    return slide;
}

std::shared_ptr<CVScene> OTSlide::getAuxImage(const std::string& sceneName) const
{
    auto it = m_auxImages.find(sceneName);
    if(it==m_auxImages.end()) {
        throw std::runtime_error(
            (boost::format("The slide does non have auxiliary image \"%1%\"") % sceneName).str()
        );
    }
    return it->second;
}

void OTSlide::log()
{
    SLIDEIO_LOG(INFO) << "---OTSlide" << std::endl;
    SLIDEIO_LOG(INFO) << "filePath:" << m_filePath << std::endl;
}
