// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/scn/scnslide.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/core/tools/xmltools.hpp"

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include "slideio/imagetools/libtiff.hpp"
#include "slideio/drivers/svs/svssmallscene.hpp"


using namespace slideio;
using namespace tinyxml2;

SCNSlide::SCNSlide(const std::string& filePath) : m_filePath(filePath)
{
    init();
}

void SCNSlide::init()
{
    namespace fs = boost::filesystem;
    if (!fs::exists(m_filePath)) {
        throw std::runtime_error(std::string("SCNImageDriver: File does not exist:") + m_filePath);
    }
    std::vector<TiffDirectory> directories;
    m_tiff = libtiff::TIFFOpen(m_filePath.c_str(), "r");
    if (!m_tiff.isValid())
    {
        throw std::runtime_error(std::string("SCNImageDriver: Cannot open file:") + m_filePath);
    }
    TiffTools::scanFile(m_tiff, directories);
    m_rawMetadata = directories[0].description;
    constructScenes();
}

void SCNSlide::constructScenes()
{
    XMLDocument doc;
    XMLError error = doc.Parse(m_rawMetadata.c_str(), m_rawMetadata.size());
    if (error != XML_SUCCESS)
    {
        throw std::runtime_error("SCNImageDriver: Error parsing metadata xml");
    }
    // begin testing block
    // doc.SaveFile("d:Temp/scn.xml", false);
    // end testing block
    std::vector<std::string> collectionPath = {"scn", "collection"};
    const XMLElement* xmlCollection = XMLTools::getElementByPath(&doc, collectionPath);
    for (auto xmlImage = xmlCollection->FirstChildElement("image");
         xmlImage != nullptr; xmlImage = xmlImage->NextSiblingElement())
    {
        const char* tagName = xmlImage->Name();
        if (tagName)
        {
            if (strcmp(tagName, "image") == 0)
            {
                std::shared_ptr<SCNScene> scene(new SCNScene(m_filePath, xmlImage));
                double magn = scene->getMagnification();
                if (magn >= 1.)
                {
                    m_Scenes.push_back(scene);
                }
                else
                {
                    std::string name(tagName);
                    if (name.compare("image") == 0)
                        name = "Macro";
                    int count = static_cast<int>(m_auxImages.size());
                    if (count > 0)
                    {
                        name += "~";
                        name += std::to_string(count);
                    }
                    m_auxImages[name] = scene;
                    m_auxNames.push_back(name);
                }
            }
            else if (strcmp(tagName, "supplementalImage") == 0)
            {
                const char *type = xmlImage->Attribute("type");
                int dir = xmlImage->IntAttribute("ifd", -1);
                if (type && dir>=0)
                {
                    slideio::TiffDirectory directory;
                    TiffTools::scanTiffDir(m_tiff.getHandle(), dir, 0, directory);
                    std::shared_ptr<CVScene> scene = std::make_shared<SVSSmallScene>(
                        m_filePath, tagName, directory, m_tiff.getHandle());
                    m_auxImages[type] = scene;
                    m_auxNames.push_back(type);
                }
            }
        }
    }
}

SCNSlide::~SCNSlide()
{
}

int SCNSlide::getNumScenes() const
{
    return (int)m_Scenes.size();
}

std::string SCNSlide::getFilePath() const
{
    return m_filePath;
}

std::shared_ptr<CVScene> SCNSlide::getScene(int index) const
{
    if(index>=getNumScenes())
        throw std::runtime_error("SCN driver: invalid m_scene index");
    return m_Scenes[index];
}

std::shared_ptr<CVScene> SCNSlide::getAuxImage(const std::string& sceneName) const
{
    auto it = m_auxImages.find(sceneName);
    if (it == m_auxImages.end()) {
        throw std::runtime_error(
            (boost::format("The slide does non have auxiliary image \"%1%\"") % sceneName).str()
        );
    }
    return it->second;
}
