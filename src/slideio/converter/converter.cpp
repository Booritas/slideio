// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/converter/converter.hpp"
#include <boost/filesystem.hpp>

#include "convertersvstools.hpp"
#include "convertertifftools.hpp"
#include "convertertools.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/imagetools/tifftools.hpp"

using namespace slideio;

#define TIFFKeeperPtr std::shared_ptr<TIFFKeeper>

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

void createZoomLevel(TIFFKeeperPtr& file, int zoomLevel, CVScenePtr scene, const cv::Size& tileSize)
{
    auto rect = scene->getRect();
    const uint32_t sceneWidth = rect.width;
    const uint32_t sceneHeight = rect.height;

    // compute scaled image size
    uint32_t imageWidth = sceneWidth >> zoomLevel;
    uint32_t imageHeight = sceneHeight >> zoomLevel;

    const uint32_t tileWidth = tileSize.width;
    const uint32_t tileHeight = tileSize.height;

    // align image size to integer number of tiles
    imageWidth = ((imageWidth - 1) / tileWidth + 1) * tileWidth;
    imageHeight = ((imageHeight - 1) / tileHeight + 1) * tileHeight;

    TiffDirectory dir;
    dir.channels = scene->getNumChannels();
    dir.dataType = scene->getChannelDataType(0);
    dir.slideioCompression = Compression::Jpeg;
    dir.width = imageWidth;
    dir.height = imageHeight;
    dir.tileWidth = tileWidth;
    dir.tileHeight = tileHeight;
    dir.description = createDescription(scene);
    dir.res = scene->getResolution();
    file->setTags(dir, zoomLevel > 0);
    
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
            ConverterTools::readTile(scene, zoomLevel, originRect, tileSize, tile);
        }
    }
}



void createSVS(TIFFKeeperPtr& file, std::shared_ptr<slideio::CVScene>& scene, int numZoomLevels, const cv::Size& tileSize)
{
    ConverterSVSTools::checkSVSRequirements(scene);
    for (int zoomLevel = 0; zoomLevel < numZoomLevels; ++zoomLevel) {
        createZoomLevel(file, zoomLevel, scene, tileSize);
    }
}


static void convertToSVS(CVScenePtr scene,
                         const std::map<std::string,std::string>& parameters,
                         const std::string& outputPath)
{
    bool success = false;

    TIFFKeeperPtr file(new TIFFKeeper(outputPath, false));

    const cv::Size tileSize(256, 256);

    auto rect = scene->getRect();
    const int imageWidth = rect.width;
    const int imageHeight = rect.height;

    const int numZoomLevels = ConverterTools::computeNumZoomLevels(imageWidth, imageHeight);

    createSVS(file, scene, numZoomLevels, tileSize);

    file->closeTiffFile();

    if(!success) {
        boost::filesystem::remove(outputPath);
    }
}
