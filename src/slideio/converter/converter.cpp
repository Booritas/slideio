// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/converter/converter.hpp"
#include <boost/filesystem.hpp>
#include "slideio/imagetools/libtiff.hpp"

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

void convertToSVS(const std::shared_ptr<slideio::Scene>& scene,
    const std::map<std::string,
    std::string>& parameters,
    const std::string& outputPath)
{
    const uint32_t tileWidth = 256;
    const uint32_t tileHeight = 256;
    bool success = false;
    libtiff::TIFF* tiff = libtiff::TIFFOpen(outputPath.c_str(), "w");
    if(!tiff) {
        RAISE_RUNTIME_ERROR << "Converter: cannot create output file '" << outputPath << "'.";
    }
    auto rect = scene->getRect();
    uint32_t imageWidth = std::get<2>(rect);
    uint32_t imageHeight = std::get<3>(rect);

    libtiff::TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, 1);
    libtiff::TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, 1);
    libtiff::TIFFSetField(tiff, TIFFTAG_COMPRESSION, 1);
    libtiff::TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, imageWidth);
    libtiff::TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, imageHeight);
    libtiff::TIFFSetField(tiff, TIFFTAG_TILEWIDTH, tileWidth);
    libtiff::TIFFSetField(tiff, TIFFTAG_TILELENGTH, tileHeight);
    libtiff::TIFFSetField(tiff, TIFFTAG_IMAGEDESCRIPTION, 1);
    libtiff::TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, 1);
    libtiff::TIFFSetField(tiff, TIFFTAG_XRESOLUTION, 1);
    libtiff::TIFFSetField(tiff, TIFFTAG_YRESOLUTION, 1);
    libtiff::TIFFSetField(tiff, TIFFTAG_RESOLUTIONUNIT, 1);
    libtiff::TIFFSetField(tiff, TIFFTAG_XPOSITION, 1);
    libtiff::TIFFSetField(tiff, TIFFTAG_YPOSITION, 1);
    libtiff::TIFFSetField(tiff, TIFFTAG_DATATYPE, 1);
    libtiff::TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, 1);

    libtiff::TIFFClose(tiff);
    if(!success) {
        boost::filesystem::remove(outputPath);
    }
}
