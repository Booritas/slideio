// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/converter/converter.hpp"
#include <boost/filesystem.hpp>

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


void createZoomLevel(libtiff::TIFF* tiff, const TiffDirectory& dir, ScenePtr scene)
{
    if(dir.dirIndex>0) {
        libtiff::TIFFWriteDirectory(tiff);
    }
    libtiff::TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, 1);
    libtiff::TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, 1);
    libtiff::TIFFSetField(tiff, TIFFTAG_COMPRESSION, 1);
    libtiff::TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, dir.width);
    libtiff::TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, dir.height);
    libtiff::TIFFSetField(tiff, TIFFTAG_TILEWIDTH, dir.tileWidth);
    libtiff::TIFFSetField(tiff, TIFFTAG_TILELENGTH, dir.tileHeight);
    libtiff::TIFFSetField(tiff, TIFFTAG_IMAGEDESCRIPTION, 1);
    libtiff::TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, 1);
    libtiff::TIFFSetField(tiff, TIFFTAG_XRESOLUTION, 1);
    libtiff::TIFFSetField(tiff, TIFFTAG_YRESOLUTION, 1);
    libtiff::TIFFSetField(tiff, TIFFTAG_RESOLUTIONUNIT, 1);
    libtiff::TIFFSetField(tiff, TIFFTAG_XPOSITION, 1);
    libtiff::TIFFSetField(tiff, TIFFTAG_YPOSITION, 1);
    libtiff::TIFFSetField(tiff, TIFFTAG_DATATYPE, 1);
    libtiff::TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, 1);

}

void createSVS(libtiff::TIFF* tiff, const std::vector<slideio::TiffDirectory>& dirs, ScenePtr scene)
{
    for(auto dir:dirs) {
        createZoomLevel(tiff, dir, scene);
    }
}

void convertToSVS(const std::shared_ptr<slideio::Scene>& scene,
                  const std::map<std::string,
                                 std::string>& parameters,
                  const std::string& outputPath)
{
    const uint32_t tileWidth = 256;
    const uint32_t tileHeight = 256;
    const int numZoomLevels = 1;
    bool success = false;

    libtiff::TIFF* tiff = libtiff::TIFFOpen(outputPath.c_str(), "w");
    if (!tiff) {
        RAISE_RUNTIME_ERROR << "Converter: cannot create output file '" << outputPath << "'.";
    }
    auto rect = scene->getRect();
    uint32_t imageWidth = std::get<2>(rect);
    uint32_t imageHeight = std::get<3>(rect);

    std::vector<slideio::TiffDirectory> directories(numZoomLevels);

    uint32_t currentDirWidth = imageWidth;
    uint32_t currentDirHeight = imageHeight;
    std::tuple<double, double> currentRes = scene->getResolution();
    double xRes = std::get<0>(currentRes)/2.;
    double yRes = std::get<1>(currentRes)/2.;
    for(int zoomLevel=0; zoomLevel<numZoomLevels; ++zoomLevel) {
        slideio::TiffDirectory& dir = directories.at(zoomLevel);
        dir.width = currentDirWidth;
        dir.height = currentDirHeight;
        dir.tileWidth = tileWidth;
        dir.height = tileHeight;
        dir.channels = scene->getNumChannels();
        dir.dataType = scene->getChannelDataType(0);
        dir.dirIndex = zoomLevel;
        dir.tiled = true;
        dir.res.x = xRes;
        dir.res.y = yRes;
        currentDirWidth /= 2;
        currentDirHeight /= 2;
        xRes /= 2.;
        yRes /= 2.;
    }
    createSVS(tiff, directories, scene);

    libtiff::TIFFClose(tiff);
    if(!success) {
        boost::filesystem::remove(outputPath);
    }
}
