// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/scn/scnscene.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include <tiffio.h>

#include "slideio/xmltools.hpp"

using namespace slideio;
using namespace tinyxml2;

SCNScene::SCNScene(const std::string& filePath, const tinyxml2::XMLElement* xmlImage):
    m_filePath(filePath),
    m_compression(Compression::Unknown),
    m_resolution(0., 0.),
    m_dataType(slideio::DataType::DT_Unknown),
    m_magnification(0.)
{
    init(xmlImage);
}

SCNScene::~SCNScene()
{
}

cv::Rect SCNScene::getRect() const
{
    return m_rect;
}

int SCNScene::getNumChannels() const
{
    return m_numChannels;
}

void SCNScene::readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& channelIndices, cv::OutputArray output)
{
}

void SCNScene::init(const XMLElement* xmlImage)
{
    const char* name = xmlImage->Attribute("name");
    m_name = name ? name : "unknown";
    XMLPrinter printer;
    xmlImage->Accept(&printer);
    std::stringstream imageDoc;
    imageDoc << printer.CStr();
    m_reawMetadata = imageDoc.str();
    const XMLElement* xmlPixels = xmlImage->FirstChildElement("pixels");
    m_rect.width = xmlPixels->IntAttribute("sizeX");
    m_rect.height = xmlPixels->IntAttribute("sizeY");
}
