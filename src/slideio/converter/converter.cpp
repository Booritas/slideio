// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/converter/converter.hpp"
#include <boost/filesystem.hpp>

#include "convertertools.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/imagetools/libtiff.hpp"
#include "slideio/imagetools/tifftools.hpp"

using namespace slideio;

void convertToSVS(CVScenePtr scene,
                const std::map<std::string, std::string>& parameters, 
                const std::string& outputPath);

void slideio::convertScene(ScenePtr scene,
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
    convertToSVS(scene->getCVScene(), parameters, outputPath);
}


static uint16_t bitsPerSampleFromScene(CVScenePtr scene)
{
    DataType dt = scene->getChannelDataType(0);
    int ds = ImageTools::dataTypeSize(dt);
    return (uint16_t)(ds * 8);
}

static uint16_t compressionFromScene(CVScenePtr scene)
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


static uint16_t tiffDataTypeFromScene(CVScenePtr scene)
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

uint16_t photometricFromScene(CVScenePtr scene)
{
    const DataType dt = scene->getChannelDataType(0);
    const int numChannels = scene->getNumChannels();
    uint16_t photometric = PHOTOMETRIC_SEPARATED;
    if(dt==DataType::DT_Byte && numChannels==3) {
        photometric = PHOTOMETRIC_RGB;
    }
    return photometric;
}

static std::string createDescription(CVScenePtr scene)
{
    auto rect = scene->getRect();
    std::stringstream buff;
    buff << "SlideIO Library 2.0" << std::endl;
    buff << rect.width << "x" << rect.height << std::endl;
    double magn = scene->getMagnification();
    if(magn>0) {
        buff << "AppMag = " << magn;
    }
    return buff.str();
}

static void readTile(CVScenePtr scene,
                        int zoomLevel,
                        const cv::Rect& tileRect,
                        const cv::Size& tileSize,
                        cv::OutputArray tile)
{
    auto sceneRect = scene->getRect();
    int lastX = tileRect.x + tileRect.width;
    int lastY = tileRect.y + tileRect.height;
    int diffX = sceneRect.width - lastX;
    int diffY = sceneRect.height - lastY;
    int orgTileWidth = tileRect.width;
    int orgTileHeight = tileRect.height;
    int tileWidth = tileSize.width;
    int tileHeight = tileSize.height;

    if(diffX<0) {
        orgTileWidth += diffX;
        tileWidth = orgTileWidth >> zoomLevel;
    }
    if(diffY<0) {
        orgTileHeight += diffY;
        tileHeight = orgTileHeight >> zoomLevel;
    }

    cv::Rect adjustedRect(tileRect.x, tileRect.y, orgTileWidth, orgTileHeight);
    cv::Size adjustedTileSize(tileWidth,tileHeight);

    scene->readResampledBlock(adjustedRect, adjustedTileSize, tile);
}

void createZoomLevel(libtiff::TIFF* tiff, int zoomLevel, CVScenePtr scene, const cv::Size& tileSize)
{
    if (zoomLevel > 0) {
        libtiff::TIFFWriteDirectory(tiff);
    }
    double scaling = 1./(double(2 > zoomLevel));
    auto rect = scene->getRect();
    const uint32_t imageWidth = (uint32_t)std::lround(double(rect.width)*scaling);
    const uint32_t imageHeight = (uint32_t)std::lround(double(rect.height)*scaling);
    const uint32_t tileWidth = tileSize.width;
    const uint32_t tileHeight = tileSize.height;

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
    auto res = scene->getResolution();
    libtiff::TIFFSetField(tiff, TIFFTAG_XRESOLUTION, (float)res.x);
    libtiff::TIFFSetField(tiff, TIFFTAG_YRESOLUTION, (float)res.y);
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
    cv::Mat tile;
    for(uint32_t y=0; y<imageHeight; y+=tileHeight, originY+=originTileHeight) {
        for(uint32_t x=0; x<imageWidth; x+=tileWidth, originX+=originTileWidth) {
            cv::Point tilePos(x, y);
            cv::Rect originRect(originX, originY, originTileWidth, originTileHeight);
            readTile(scene, zoomLevel, originRect, tileSize, tile);
        }
    }
}


void checkSVSRequirements(std::shared_ptr<slideio::CVScene>& scene)
{
    const DataType dt = scene->getChannelDataType(0);
    const int numChannels = scene->getNumChannels();
    for(int channel=1; channel<numChannels; ++channel) {
        if(dt != scene->getChannelDataType(channel)) {
            RAISE_RUNTIME_ERROR << "Converter: Cannot convert scene with different channel types to SVS";
        }
    }
}

void createSVS(libtiff::TIFF* tiff, std::shared_ptr<slideio::CVScene>& scene, int numZoomLevels, const cv::Size& tileSize)
{
    checkSVSRequirements(scene);
    for (int zoomLevel = 0; zoomLevel < numZoomLevels; ++zoomLevel) {
        createZoomLevel(tiff, zoomLevel, scene, tileSize);
    }
}


static void convertToSVS(CVScenePtr scene,
                         const std::map<std::string,std::string>& parameters,
                         const std::string& outputPath)
{
    bool success = false;

    libtiff::TIFF* tiff = libtiff::TIFFOpen(outputPath.c_str(), "w");
    if (!tiff) {
        RAISE_RUNTIME_ERROR << "Converter: cannot create output file '" << outputPath << "'.";
    }

    const cv::Size tileSize(256, 256);

    auto rect = scene->getRect();
    const int imageWidth = rect.width;
    const int imageHeight = rect.height;

    const int numZoomLevels = ConverterTools::computeNumZoomLevels(imageWidth, imageHeight);

    createSVS(tiff, scene, numZoomLevels, tileSize);

    libtiff::TIFFClose(tiff);
    if(!success) {
        boost::filesystem::remove(outputPath);
    }
}
