// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/scn/scnslide.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/xmltools.hpp"

#include <boost/filesystem.hpp>
#include <tiffio.h>


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
    m_tiff = TIFFOpen(m_filePath.c_str(), "r");
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
    std::vector<std::string> collectionPath = {"scn", "collection"};
    const XMLElement* xmlCollection = XMLTools::getElementByPath(&doc, collectionPath);
    for (auto xmlImage = xmlCollection->FirstChildElement("image");
        xmlImage != nullptr; xmlImage = xmlImage->NextSiblingElement())
    {
        std::shared_ptr<SCNScene> scene(new SCNScene(m_filePath, xmlImage));
        m_Scenes.push_back(scene);
    }
    //doc.SaveFile("D:/Temp/scn.xml");
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

