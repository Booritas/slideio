// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/scn/scnscene.hpp"

#include <set>

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

std::string SCNScene::getChannelName(int channel) const
{
    return m_channelNames[channel];
}

void SCNScene::init(const XMLElement* xmlImage)
{
    struct DimensionInfo
    {
        int width;
        int height;
        int r;
        int c;
        int ifd;
    };

    m_tiff = TIFFOpen(m_filePath.c_str(), "r");
    if (!m_tiff.isValid())
    {
        throw std::runtime_error(std::string("SCNImageDriver: Cannot open file:") + m_filePath);
    }

    std::vector<DimensionInfo> dimensions;
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
    int maxChannelIndex = -1;
    for (const auto* xmlDimension = xmlPixels->FirstChildElement("dimension");
         xmlDimension != nullptr; xmlDimension = xmlDimension->NextSiblingElement())
    {
        DimensionInfo dim = {};
        dim.width = xmlDimension->IntAttribute("sizeX", -1);
        dim.height = xmlDimension->IntAttribute("sizeY", -1);
        dim.r = xmlDimension->IntAttribute("r", -1);
        dim.c = xmlDimension->IntAttribute("c",-1);
        dim.ifd = xmlDimension->IntAttribute("ifd", -1);
        dimensions.push_back(dim);
        if(dim.c >0)
            maxChannelIndex = std::max(maxChannelIndex, dim.c);
    }
    auto itDim = std::find_if(dimensions.begin(), dimensions.end(), [](const DimensionInfo& info)
    {
            return info.r == 0;
    });
    if (itDim == dimensions.end())
    {
        throw std::runtime_error("SCNImageDriver: invalid scene specification. Cannot find base resolution layer");
    }
    TiffDirectory dir;
    TiffTools::scanTiffDir(m_tiff, itDim->ifd, 0, dir);
    if(maxChannelIndex>0)
    {
        m_numChannels = maxChannelIndex + 1;
    }
    else
    {
        m_numChannels = dir.channels;
    }
    m_channelNames.resize(m_numChannels);
    const std::vector<std::string> channelSettingsPath = {"scanSettings", "channelSettings"};
    const XMLElement* xmlChannelSettings = XMLTools::getElementByPath(xmlImage, channelSettingsPath);
    if(xmlChannelSettings)
    {
        for (const auto* xmlChannel = xmlChannelSettings->FirstChildElement("channel");
            xmlChannel != nullptr; xmlChannel = xmlChannel->NextSiblingElement())
        {
            const char * name = xmlChannel->Attribute("name");
            if(name)
            {
                const int channelIndex = xmlChannel->IntAttribute("index", -1);
                if(channelIndex>=0)
                {
                    m_channelNames[channelIndex] = name;
                }
            }
        }
    }
    const std::vector<std::string> objectivePath = { "scanSettings", "objectiveSettings", "objective" };
    const XMLElement* xmlObjective = XMLTools::getElementByPath(xmlImage, objectivePath);
    if(xmlObjective)
    {
        m_magnification = xmlObjective->DoubleText(0);
    }

    const tinyxml2::XMLElement* xmlView = xmlImage->FirstChildElement("view");
    if(xmlView)
    {
        const int widthNm = xmlView->IntAttribute("sizeX");
        const int heightNm = xmlView->IntAttribute("sizeY");
        const int xNm = xmlView->IntAttribute("offsetX");
        const int yNm = xmlView->IntAttribute("offsetY");
        const double pixelWidthNm = (double)widthNm / (double)m_rect.width;
        const double pixelHeightNm = (double)heightNm / (double)m_rect.height;
        m_resolution.x = 1.e-9 * pixelWidthNm;
        m_resolution.y = 1.e-9 * pixelHeightNm;
        if (pixelWidthNm>0)
            m_rect.x = (int)std::round(xNm / pixelWidthNm);
        if (pixelHeightNm>0)
            m_rect.y = (int)std::round(yNm / pixelHeightNm);
    }
}
