// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/pke/pkeslide.hpp"

#include <fstream>
#include <tinyxml2.h>

#include "slideio/imagetools/imagetools.hpp"
#include "slideio/drivers/pke/pkesmallscene.hpp"
#include "slideio/drivers/pke/pketiledscene.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/base/base.hpp"

#include <boost/filesystem.hpp>
#include <boost/format.hpp>


using namespace slideio;

const char* THUMBNAIL = "Thumbnail";
const char* MACRO = "Macro";
const char* LABEL = "Label";


PKESlide::PKESlide()
{
}

PKESlide::~PKESlide()
{
}

int PKESlide::getNumScenes() const
{
    return (int)m_Scenes.size();
}

std::string PKESlide::getFilePath() const
{
    return m_filePath;
}

std::shared_ptr<CVScene> PKESlide::getScene(int index) const
{
    if(index>=getNumScenes())
        throw std::runtime_error("PKE driver: invalid m_scene index");
    return m_Scenes[index];
}

std::shared_ptr<PKESlide> PKESlide::openFile(const std::string& filePath)
{
    SLIDEIO_LOG(INFO) << "PKESlide::openFile: " << filePath;
    std::shared_ptr<PKESlide> slide;
    std::vector<TiffDirectory> directories;
    libtiff::TIFF* tiff(nullptr);
    tiff = TiffTools::openTiffFile(filePath);
    if(!tiff) {
        SLIDEIO_LOG(WARNING) << "PKESlide::openFile: cannot open file " << filePath << " with libtiff";
        return slide;
    }
    TIFFKeeper keeper(tiff);

    TiffTools::scanFile(tiff, directories);

    std::vector<TiffDirectory> image_dirs;
    std::map<std::string, std::shared_ptr<CVScene>> auxImages;
    std::list<std::string> auxNames;
    std::string metadata;

    for (const auto& directory : directories) {
        const auto& description = directory.description;
        if (!description.empty()) {
            tinyxml2::XMLDocument doc;
            tinyxml2::XMLError error = doc.Parse(description.c_str(), description.size());
            if (error != tinyxml2::XML_SUCCESS) {
                RAISE_RUNTIME_ERROR << "PKEImageDriver: Error parsing image description xml: " << static_cast<int>(error);
            }
            tinyxml2::XMLElement* root = doc.RootElement();
            if(!root) {
                RAISE_RUNTIME_ERROR << "PKEImageDriver: Error parsing image description xml: root element is null";
            }
            auto xmlImageType= root->FirstChildElement("ImageType");
            if(xmlImageType) {
                std::string name;
                std::string type = xmlImageType->GetText();
                if(type == "FullResolution" || type == "ReducedResolution") {
                    if(metadata.empty() && type == "FullResolution") {
                        metadata = description;
                    }
                    image_dirs.push_back(directory);
                } else if(type == "Thumbnail" || type == "Overview" || type == "Label") {
                    std::shared_ptr<CVScene> scene(new PKESmallScene(filePath, type, directory, true));
                    auxImages[type] = scene;
                    auxNames.emplace_back(type);
                }
            }
        }
    }

    std::shared_ptr<CVScene> scene(new PKETiledScene(filePath,keeper.release(),"Image", image_dirs));
    std::vector<std::shared_ptr<CVScene>> scenes;
    scenes.push_back(scene);
    slide.reset(new PKESlide);
    slide->m_Scenes.assign(scenes.begin(), scenes.end());
    slide->m_filePath = filePath;
    slide->m_auxImages = auxImages;
    slide->m_auxNames = auxNames;
    slide->m_rawMetadata = metadata;

    return slide;
}

std::shared_ptr<CVScene> PKESlide::getAuxImage(const std::string& sceneName) const
{
    auto it = m_auxImages.find(sceneName);
    if(it==m_auxImages.end()) {
        throw std::runtime_error(
            (boost::format("The slide does non have auxiliary image \"%1%\"") % sceneName).str()
        );
    }
    return it->second;
}

void PKESlide::log()
{
    SLIDEIO_LOG(INFO) << "---PKESlide" << std::endl;
    SLIDEIO_LOG(INFO) << "filePath:" << m_filePath << std::endl;
}
