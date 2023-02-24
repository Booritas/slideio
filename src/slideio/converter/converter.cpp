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


void createSVS(TIFFKeeperPtr& file, std::shared_ptr<slideio::CVScene>& scene, int numZoomLevels, const cv::Size& tileSize)
{
    ConverterSVSTools::checkSVSRequirements(scene);
    for (int zoomLevel = 0; zoomLevel < numZoomLevels; ++zoomLevel) {
        ConverterTools::createZoomLevel(file, zoomLevel, scene, tileSize);
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
