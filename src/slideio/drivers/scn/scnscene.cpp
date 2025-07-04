// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/scn/scnscene.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/core/tools/xmltools.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/imagetools/libtiff.hpp"


using namespace slideio;
using namespace tinyxml2;

SCNScene::SCNScene(const std::string& filePath, const tinyxml2::XMLElement* xmlImage):
    m_filePath(filePath),
    m_compression(Compression::Unknown),
    m_resolution(0., 0.),
    m_magnification(0.),
    m_interleavedChannels(false),
    m_numChannels(1),
    m_numZSlices(1),
    m_planeCount(1)
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

void SCNScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& channelIndicesIn, int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
	if (tFrameIndex != 0) {
		throw std::runtime_error("SCNImageDriver: Time frames are not supported");
	}
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
        auto directories = getChannelDirectories(channelIndex, zSliceIndex);
        if(directories.empty()) {
            continue;
        }
        const slideio::TiffDirectory& dir = findZoomDirectory(channelIndex, zSliceIndex, zoom);
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
        dim.z = xmlDimension->IntAttribute("z", -1);
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
         DataType dataType = getChannelDirectories(channelIndex,0)[0].dataType;
         m_channelDataType[channelIndex] = dataType==DataType::DT_None?DataType::DT_Byte:dataType;
    }
}

void SCNScene::setupChannels(const XMLElement* xmlImage)
{
    const XMLElement* xmlPixels = xmlImage->FirstChildElement("pixels");
    int maxChannelIndex = -1;
    int maxZIndex = -1;
    std::vector<SCNDimensionInfo> dimensions = parseDimensions(xmlPixels);
    std::for_each(dimensions.begin(), dimensions.end(), 
                  [&maxChannelIndex, &maxZIndex](const SCNDimensionInfo& dim){
                      maxChannelIndex = std::max(dim.c, maxChannelIndex);
                      maxZIndex = std::max(dim.z, maxZIndex);
                  });

    if (maxZIndex > 0) {
        m_numZSlices = maxZIndex + 1;
    }

    m_planeCount = std::max(1, maxChannelIndex + 1);
    m_channelDirectories.resize(m_planeCount*m_numZSlices);

    for(auto & dim: dimensions) {
        int channel = dim.c < 0 ? 0 : dim.c;
        int zIndex = dim.z < 0 ? 0 : dim.z;
        TiffDirectory channelDir;
        TiffTools::scanTiffDir(m_tiff, dim.ifd, 0, channelDir);
        m_channelDirectories[m_planeCount*zIndex + channel].push_back(channelDir);
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
    const auto& directories = getChannelDirectories(0,0);
    if (!directories.empty()) {
        const int numLevels = static_cast<int>(directories.size());
        const int width0 = directories[0].width;
        m_levels.resize(directories.size());
        for (int lv = 0; lv < numLevels; ++lv) {
            const TiffDirectory& directory = directories[lv];
            LevelInfo& level = m_levels[lv];
            const double scale = static_cast<double>(directory.width) / static_cast<double>(width0);
            level.setLevel(lv);
            level.setScale(scale);
            level.setSize({ directory.width, directory.height });
            level.setTileSize({ directory.tileWidth, directory.tileHeight });
            level.setMagnification(m_magnification * scale);
        }
    }
}

const TiffDirectory& SCNScene::findZoomDirectory(int channelIndex, int zIndex, double zoom) const 
{
    const cv::Rect sceneRect = getRect();
    const double sceneWidth = static_cast<double>(sceneRect.width);
    const auto& directories = getChannelDirectories(channelIndex, zIndex);
    int index = Tools::findZoomLevel(zoom, (int)directories.size(), [&directories, sceneWidth](int index) {
        return directories[index].width / sceneWidth;
        });
    if(index < 0 || index >= (int)directories.size()) {
        RAISE_RUNTIME_ERROR << "SCNImageDriver: Cannot detect zoom level for channel: " << channelIndex << " z-slice index: " << zIndex;
    }
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
        int channel = 0;
        for(int channelIndex:channelIndices)
        {
            auto it = info->channel2ifd.find(channelIndex);
            if (it == info->channel2ifd.end()) {
                RAISE_RUNTIME_ERROR << "SCNImageDriver: invalid channel index " 
                    << channelIndex << " received during tile reading. File " << m_filePath;
            }
            const TiffDirectory* dir = it->second;
            TiffTools::readTile(getFileHandle(), *dir, tileIndex, localChannelIndices, channelRasters[channel]);
            ++channel;
        }
        cv::merge(channelRasters, tileRaster);
    }
    return true;
}

void SCNScene::initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices, cv::OutputArray output)
{
    initializeSceneBlock(blockSize, channelIndices, output);
}


