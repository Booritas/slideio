// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/scn/scnscene.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/core/tools/xmltools.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/imagetools/libtiff.hpp"
#include <boost/format.hpp>


using namespace slideio;
using namespace tinyxml2;

SCNScene::SCNScene(const std::string& filePath, const tinyxml2::XMLElement* xmlImage):
    m_filePath(filePath),
    m_compression(Compression::Unknown),
    m_resolution(0., 0.),
    m_magnification(0.),
    m_interleavedChannels(false)
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
                                          const std::vector<int>& channelIndicesIn, cv::OutputArray output)
{
    auto hFile = getFileHandle();
    if (hFile == nullptr)
        throw std::runtime_error("SCNImageDriver: Invalid file handle by raster reading operation");
    double zoomX = static_cast<double>(blockSize.width) / static_cast<double>(blockRect.width);
    double zoomY = static_cast<double>(blockSize.height) / static_cast<double>(blockRect.height);
    double zoom = std::max(zoomX, zoomY);
    SCNTilingInfo info;
    double zoomDirX(-1), zoomDirY(-1);

    auto channelIndices(channelIndicesIn);
    if(channelIndices.empty())
    {
        for(int channelIndex=0; channelIndex<m_numChannels; ++channelIndex)
        {
            channelIndices.push_back(channelIndex);
        }
    }

    for(auto channelIndex: channelIndices)
    {
        const slideio::TiffDirectory& dir = findZoomDirectory(channelIndex, zoom);
        info.channel2ifd[channelIndex] = &dir;
        if(zoomDirX<0 || zoomDirY<0)
        {
            zoomDirX = static_cast<double>(dir.width) / static_cast<double>(m_rect.width);
            zoomDirY = static_cast<double>(dir.height) / static_cast<double>(m_rect.height);
        }
    }

    cv::Rect resizedBlock;
    Tools::scaleRect(blockRect, zoomDirX, zoomDirY, resizedBlock);
    TileComposer::composeRect(this, channelIndices, resizedBlock, blockSize, output, (void*)&info);
}

std::string SCNScene::getChannelName(int channel) const
{
    return m_channelNames[channel];
}

std::vector<SCNDimensionInfo> SCNScene::parseDimensions(const XMLElement* xmlPixels)
{
    std::vector<SCNDimensionInfo> dimensions;
    for (const auto* xmlDimension = xmlPixels->FirstChildElement("dimension");
         xmlDimension != nullptr; xmlDimension = xmlDimension->NextSiblingElement())
    {
        SCNDimensionInfo dim = {};
        dim.width = xmlDimension->IntAttribute("sizeX", -1);
        dim.height = xmlDimension->IntAttribute("sizeY", -1);
        dim.r = xmlDimension->IntAttribute("r", -1);
        dim.c = xmlDimension->IntAttribute("c", -1);
        dim.ifd = xmlDimension->IntAttribute("ifd", -1);
        dimensions.push_back(dim);
    }
    return dimensions;
}

void SCNScene::parseGeometry(const XMLElement* xmlImage)
{
    const XMLElement* xmlPixels = xmlImage->FirstChildElement("pixels");
    m_rect.width = xmlPixels->IntAttribute("sizeX");
    m_rect.height = xmlPixels->IntAttribute("sizeY");

    const tinyxml2::XMLElement* xmlView = xmlImage->FirstChildElement("view");
    if (xmlView)
    {
        const int widthNm = xmlView->IntAttribute("sizeX");
        const int heightNm = xmlView->IntAttribute("sizeY");
        const int xNm = xmlView->IntAttribute("offsetX");
        const int yNm = xmlView->IntAttribute("offsetY");
        const double pixelWidthNm = (double)widthNm / (double)m_rect.width;
        const double pixelHeightNm = (double)heightNm / (double)m_rect.height;
        m_resolution.x = 1.e-9 * pixelWidthNm;
        m_resolution.y = 1.e-9 * pixelHeightNm;
        if (pixelWidthNm > 0)
            m_rect.x = (int)std::round(xNm / pixelWidthNm);
        if (pixelHeightNm > 0)
            m_rect.y = (int)std::round(yNm / pixelHeightNm);
    }
}

void SCNScene::parseChannelNames(const XMLElement* xmlImage)
{
    m_channelNames.resize(m_numChannels);
    const std::vector<std::string> channelSettingsPath = { "scanSettings", "channelSettings" };
    const XMLElement* xmlChannelSettings = XMLTools::getElementByPath(xmlImage, channelSettingsPath);
    if (xmlChannelSettings)
    {
        for (const auto* xmlChannel = xmlChannelSettings->FirstChildElement("channel");
             xmlChannel != nullptr; xmlChannel = xmlChannel->NextSiblingElement())
        {
            const char* name = xmlChannel->Attribute("name");
            if (name)
            {
                const int channelIndex = xmlChannel->IntAttribute("index", -1);
                if (channelIndex >= 0)
                {
                    m_channelNames[channelIndex] = name;
                }
            }
        }
    }
}

void SCNScene::parseMagnification(const XMLElement* xmlImage)
{
    const std::vector<std::string> objectivePath = { "scanSettings", "objectiveSettings", "objective" };
    const XMLElement* xmlObjective = XMLTools::getElementByPath(xmlImage, objectivePath);
    if (xmlObjective) {
        m_magnification = xmlObjective->DoubleText(0);
    }
}

void SCNScene::defineChannelDataType()
{
    m_channelDataType.resize(m_numChannels);
    for (int channelIndex = 0; channelIndex < m_numChannels; ++channelIndex)
    {
         DataType dataType = getChannelDirectories(channelIndex)[0].dataType;
         m_channelDataType[channelIndex] = dataType==DataType::DT_None?DataType::DT_Byte:dataType;
    }
}

void SCNScene::setupChannels(const XMLElement* xmlImage)
{
    const XMLElement* xmlPixels = xmlImage->FirstChildElement("pixels");
    int maxChannelIndex = -1;
    std::vector<SCNDimensionInfo> dimensions = parseDimensions(xmlPixels);
    std::for_each(dimensions.begin(), dimensions.end(), 
                  [&maxChannelIndex](const SCNDimensionInfo& dim){
                      maxChannelIndex = std::max(dim.c, maxChannelIndex);
                  });

    m_channelDirectories.resize(std::max(1, maxChannelIndex + 1));
    for(auto & dim: dimensions) {
        int channel = dim.c < 0 ? 0 : dim.c;
        TiffDirectory channelDir;
        TiffTools::scanTiffDir(m_tiff, dim.ifd, 0, channelDir);
        m_channelDirectories[channel].push_back(channelDir);
    }

    for(auto & dirs:m_channelDirectories) {
        std::sort(dirs.begin(), dirs.end(), [](const TiffDirectory& left, const TiffDirectory& right)->bool {
            return left.width > right.width;
        });
    }

    if (maxChannelIndex > 0) {
        m_numChannels = maxChannelIndex + 1;
    }
    else {
        m_numChannels = m_channelDirectories[0][0].channels;
        m_interleavedChannels = true;
    }
}

void SCNScene::init(const XMLElement* xmlImage)
{
    m_tiff = TiffTools::openTiffFile(m_filePath.c_str());
    if (!m_tiff.isValid())
    {
        throw std::runtime_error(std::string("SCNImageDriver: Cannot open file:") + m_filePath);
    }

    
    const char* name = xmlImage->Attribute("name");
    m_name = name ? name : "unknown";
    XMLPrinter printer;
    xmlImage->Accept(&printer);
    std::stringstream imageDoc;
    imageDoc << printer.CStr();
    m_reawMetadata = imageDoc.str();

    parseGeometry(xmlImage);
    setupChannels(xmlImage);
    parseChannelNames(xmlImage);
    parseMagnification(xmlImage);
    parseChannelNames(xmlImage);
    defineChannelDataType();
}

const TiffDirectory& SCNScene::findZoomDirectory(int channelIndex, double zoom) const 
{
    const cv::Rect sceneRect = getRect();
    const double sceneWidth = static_cast<double>(sceneRect.width);
    const auto& directories = getChannelDirectories(channelIndex);
    int index = Tools::findZoomLevel(zoom, (int)directories.size(), [&directories, sceneWidth](int index) {
        return directories[index].width / sceneWidth;
        });
    return directories[index];
}


int SCNScene::getTileCount(void* userData)
{
    const SCNTilingInfo* info = (const SCNTilingInfo*)userData;
    const TiffDirectory* dir = info->channel2ifd.begin()->second;
    int tilesX = (dir->width - 1) / dir->tileWidth + 1;
    int tilesY = (dir->height - 1) / dir->tileHeight + 1;
    return tilesX * tilesY;
}

bool SCNScene::getTileRect(int tileIndex, cv::Rect& tileRect, void* userData)
{
    const SCNTilingInfo* info = (const SCNTilingInfo*)userData;
    const TiffDirectory* dir = info->channel2ifd.begin()->second;
    const int tilesX = (dir->width - 1) / dir->tileWidth + 1;
    const int tilesY = (dir->height - 1) / dir->tileHeight + 1;
    const int tileY = tileIndex / tilesX;
    const int tileX = tileIndex % tilesX;
    tileRect.x = tileX * dir->tileWidth;
    tileRect.y = tileY * dir->tileHeight;
    tileRect.width = dir->tileWidth;
    tileRect.height = dir->tileHeight;
    return true;
}

bool SCNScene::readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
    void* userData)
{
    const SCNTilingInfo* info = (const SCNTilingInfo*)userData;
    if(m_interleavedChannels)
    {
        const TiffDirectory* dir = info->channel2ifd.begin()->second;
        TiffTools::readTile(getFileHandle(), *dir, tileIndex, channelIndices, tileRaster);
    }
    else if(channelIndices.size()==1)
    {
        const std::vector<int> localChannelIndices = { 0 };
        const TiffDirectory* dir = info->channel2ifd.begin()->second;
        TiffTools::readTile(getFileHandle(), *dir, tileIndex, localChannelIndices, tileRaster);
    }
    else
    {
        const std::vector<int> localChannelIndices = { 0 };
        std::vector<cv::Mat> channelRasters;
        channelRasters.resize(channelIndices.size());
        for(int channelIndex:channelIndices)
        {
            auto it = info->channel2ifd.find(channelIndex);
            if (it == info->channel2ifd.end())
                throw std::runtime_error(
                    (boost::format(
                        "SCNImageDriver: invalid channel index (%1%) received during tile reading. File %2%.")
                         % channelIndex % m_filePath).str());
            const TiffDirectory* dir = it->second;
            TiffTools::readTile(getFileHandle(), *dir, tileIndex, localChannelIndices, channelRasters[channelIndex]);
        }
        cv::merge(channelRasters, tileRaster);
    }
    //{
    //    cv::Rect tileRect;
    //    getTileRect(tileIndex, tileRect, userData);
    //    const std::string path = (boost::format("D:/Temp/tiles/tile_x%1%_y%2%.png") % tileRect.x % tileRect.y).str();
    //    slideio::ImageTools::writeRGBImage(path, slideio::Compression::Png, tileRaster.getMat());
    //}
    return true;
}

void SCNScene::initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices, cv::OutputArray output)
{
    initializeSceneBlock(blockSize, channelIndices, output);
}


