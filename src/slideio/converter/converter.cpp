// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/converter/converter.hpp"
#include <boost/filesystem.hpp>

#include "convertertools.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/imagetools/libtiff.hpp"
#include "slideio/imagetools/tifftools.hpp"

using namespace slideio;

void convertToSVS(const std::shared_ptr<slideio::Scene>& scene, 
                const std::map<std::string, 
                std::string>& parameters, 
                const std::string& outputPath);

void slideio::convertScene(std::shared_ptr<slideio::Scene> scene,
                           const std::map<std::string, std::string>& parameters,
                           const std::string& outputPath)
{
    if(scene == nullptr) {
        RAISE_RUNTIME_ERROR << "Converter: invalid input scene!";
    }
    auto itDriver = parameters.find(DRIVER);
    if(itDriver == parameters.end()) {
        RAISE_RUNTIME_ERROR << "Converter: unspecified output format!";
    }
    std::string driver = itDriver->second;
    if(driver.compare("SVS")!=0) {
        RAISE_RUNTIME_ERROR << "Converter: output format '" << driver << "' is not supported!";
    }
    if(boost::filesystem::exists(outputPath)) {
        RAISE_RUNTIME_ERROR << "Converter: output file already exists.";
    }
    std::string sceneName = scene->getName();
    std::string filePath = scene->getFilePath();
    SLIDEIO_LOG(INFO) << "Convert a scene " << sceneName << " from file " << filePath << " to format: '" << driver << "'.";
    convertToSVS(scene, parameters, outputPath);
}


static uint16_t bitsPerSampleFromScene(const std::shared_ptr<slideio::Scene>& scene)
{
    DataType dt = scene->getChannelDataType(0);
    int ds = ImageTools::dataTypeSize(dt);
    return (uint16_t)(ds * 8);
}

static uint16_t compressionFromScene(const std::shared_ptr<slideio::Scene>& scene)
{
    const DataType dt = scene->getChannelDataType(0);
    const int numChannels = scene->getNumChannels();
    uint16_t compression = 0x1;

    switch(dt) {
    case DataType::DT_Byte:
        if(numChannels==1 || numChannels==3) {
            compression = 0x7;
        }
        break;
    case DataType::DT_Int8: 
    case DataType::DT_Int16:
    case DataType::DT_Float16:
    case DataType::DT_Int32:
    case DataType::DT_Float32:
    case DataType::DT_Float64:
    case DataType::DT_UInt16:
        break;
    }
    return compression;
}


static uint16_t tiffDataTypeFromScene(ScenePtr scene)
{
    const DataType dt = scene->getChannelDataType(0);
    uint16_t tiffDataType = libtiff::TIFF_NOTYPE;

    switch (dt) {
    case DataType::DT_Byte:
        tiffDataType = libtiff::TIFF_BYTE;
        break;
    case DataType::DT_Int16:
        tiffDataType = libtiff::TIFF_SSHORT;
        break;
    case DataType::DT_UInt16:
        tiffDataType = libtiff::TIFF_SHORT;
        break;
    case DataType::DT_Int8:
        tiffDataType = libtiff::TIFF_SBYTE;
        break;
    case DataType::DT_Int32:
        tiffDataType = libtiff::TIFF_SBYTE;
        break;
    case DataType::DT_Float32:
        tiffDataType = libtiff::TIFF_FLOAT;
        break;
    case DataType::DT_Float64:
        tiffDataType = libtiff::TIFF_DOUBLE;
        break;
    default:
        RAISE_RUNTIME_ERROR << "Converter: Unsupported type by TIFF: " << (int)dt;
    }

    return tiffDataType;
}

uint16_t photometricFromScene(const std::shared_ptr<slideio::Scene>& scene)
{
    const DataType dt = scene->getChannelDataType(0);
    const int numChannels = scene->getNumChannels();
    uint16_t photometric = PHOTOMETRIC_SEPARATED;
    if(dt==DataType::DT_Byte && numChannels==3) {
        photometric = PHOTOMETRIC_RGB;
    }
    return photometric;
}

static std::string createDescription(ScenePtr scene)
{
    auto rect = scene->getRect();
    std::stringstream buff;
    buff << "SlideIO Library 2.0" << std::endl;
    buff << std::get<2>(rect) << "x" << std::get<3>(rect) << std::endl;
    double magn = scene->getMagnification();
    if(magn>0) {
        buff << "AppMag = " << magn;
    }
    return buff.str();
}

static void readTile(const std::shared_ptr<slideio::Scene>& scene,
                        int zoomLevel,
                        const std::tuple<int, int, int, int>& tileRect,
                        const std::tuple<int, int>& tileSize,
                        std::vector<uint8_t>& data)
{
    auto sceneRect = scene->getRect();
    int lastX = std::get<0>(tileRect) + std::get<2>(tileRect);
    int lastY = std::get<1>(tileRect) + std::get<3>(tileRect);
    int diffX = std::get<2>(sceneRect) - lastX;
    int diffY = std::get<3>(sceneRect) - lastY;
    int orgTileWidth = std::get<2>(tileRect);
    int orgTileHeight = std::get<3>(tileRect);
    int tileWidth = std::get<0>(tileSize);
    int tileHeight = std::get<1>(tileSize);

    if(diffX<0) {
        orgTileWidth += diffX;
        tileWidth = orgTileWidth >> zoomLevel;
    }
    if(diffY<0) {
        orgTileHeight += diffY;
        tileHeight = orgTileHeight >> zoomLevel;
    }

    std::tuple<int, int, int, int>adjustedRect(std::get<0>(tileRect), std::get<1>(tileRect), orgTileWidth, orgTileHeight);
    std::tuple<int,int> adjustedTileSize(tileWidth,tileHeight);

    scene->readResampledBlock(adjustedRect, adjustedTileSize, data.data(), data.size());
}

void createZoomLevel(libtiff::TIFF* tiff, int zoomLevel, ScenePtr scene, const std::tuple<int, int>& tileSize)
{
    if (zoomLevel > 0) {
        libtiff::TIFFWriteDirectory(tiff);
    }
    double scaling = 1./(double(2 > zoomLevel));
    auto rect = scene->getRect();
    const uint32_t imageWidth = (uint32_t)std::lround(double(std::get<2>(rect))*scaling);
    const uint32_t imageHeight = (uint32_t)std::lround(double(std::get<3>(rect))*scaling);
    const uint32_t tileWidth = std::get<0>(tileSize);
    const uint32_t tileHeight = std::get<1>(tileSize);

    const uint16_t numChannels = (uint16_t)scene->getNumChannels();
    libtiff::TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, numChannels);
    const uint16_t bitsPerSample = bitsPerSampleFromScene(scene);
    libtiff::TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, bitsPerSample);
    const uint16_t compression = compressionFromScene(scene);
    libtiff::TIFFSetField(tiff, TIFFTAG_COMPRESSION, compression);
    libtiff::TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, imageWidth);
    libtiff::TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, imageHeight);
    libtiff::TIFFSetField(tiff, TIFFTAG_TILEWIDTH, tileWidth);
    libtiff::TIFFSetField(tiff, TIFFTAG_TILELENGTH, tileHeight);
    const std::string description = createDescription(scene);
    libtiff::TIFFSetField(tiff, TIFFTAG_IMAGEDESCRIPTION, description.c_str());
    libtiff::TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, 1);
    const std::tuple<double, double> res = scene->getResolution();
    libtiff::TIFFSetField(tiff, TIFFTAG_XRESOLUTION, (float)std::get<0>(res));
    libtiff::TIFFSetField(tiff, TIFFTAG_YRESOLUTION, (float)std::get<1>(res));
    libtiff::TIFFSetField(tiff, TIFFTAG_RESOLUTIONUNIT, 1);
    libtiff::TIFFSetField(tiff, TIFFTAG_XPOSITION, 0);
    libtiff::TIFFSetField(tiff, TIFFTAG_YPOSITION, 0);
    uint16_t dt = tiffDataTypeFromScene(scene);
    libtiff::TIFFSetField(tiff, TIFFTAG_DATATYPE, dt);
    uint16_t phm = photometricFromScene(scene);
    libtiff::TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, phm);

    std::tuple<int, int> imageSize(imageWidth, imageHeight);
    int originX = 0;
    int originY = 0;
    int originTileWidth = tileWidth << zoomLevel;
    int originTileHeight = tileHeight << zoomLevel;
    std::vector<uint8_t> data;
    for(uint32_t y=0; y<imageHeight; y+=tileHeight, originY+=originTileHeight) {
        for(uint32_t x=0; x<imageWidth; x+=tileWidth, originX+=originTileWidth) {
            std::tuple<int, int> tilePos(x, y);
            std::tuple<int, int, int, int> originRect(originX, originY, originTileWidth, originTileHeight);
            readTile(scene, zoomLevel, originRect, tileSize, data);
        }
    }
}


void checkSVSRequirements(const std::shared_ptr<slideio::Scene>& scene)
{
    const DataType dt = scene->getChannelDataType(0);
    const int numChannels = scene->getNumChannels();
    for(int channel=1; channel<numChannels; ++channel) {
        if(dt != scene->getChannelDataType(channel)) {
            RAISE_RUNTIME_ERROR << "Converter: Cannot convert scene with different channel types to SVS";
        }
    }
}

void createSVS(libtiff::TIFF* tiff, ScenePtr scene, int numZoomLevels, const std::tuple<int,int>& tileSize)
{
    checkSVSRequirements(scene);
    for (int zoomLevel = 0; zoomLevel < numZoomLevels; ++zoomLevel) {
        createZoomLevel(tiff, zoomLevel, scene, tileSize);
    }
}


static void convertToSVS(const std::shared_ptr<slideio::Scene>& scene,
                         const std::map<std::string,std::string>& parameters,
                         const std::string& outputPath)
{
    bool success = false;

    libtiff::TIFF* tiff = libtiff::TIFFOpen(outputPath.c_str(), "w");
    if (!tiff) {
        RAISE_RUNTIME_ERROR << "Converter: cannot create output file '" << outputPath << "'.";
    }

    const std::tuple<int, int> tileSize(256, 256);

    auto rect = scene->getRect();
    const int imageWidth = std::get<2>(rect);
    const int imageHeight = std::get<3>(rect);

    const int numZoomLevels = ConverterTools::computeNumZoomLevels(imageWidth, imageHeight);

    createSVS(tiff, scene, numZoomLevels, tileSize);

    libtiff::TIFFClose(tiff);
    if(!success) {
        boost::filesystem::remove(outputPath);
    }
}
