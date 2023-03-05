// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/converter/converter.hpp"
#include <boost/filesystem.hpp>

#include "convertersvstools.hpp"
#include "convertertools.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/imagetools/tifftools.hpp"

using namespace slideio;


static void convertToSVS(CVScenePtr scene,
                            ConverterParameters& parameters,
                            const std::string& outputPath)
{
    try {
        TIFFKeeperPtr file(new TIFFKeeper(outputPath, false));
        if (parameters.numZoomLevels <= 0) {
            auto rect = scene->getRect();
            parameters.numZoomLevels = ConverterTools::computeNumZoomLevels(rect.width, rect.height);
        }
        ConverterSVSTools::createSVS(file, scene, parameters);
    }
    catch(std::exception&) {
        boost::filesystem::remove(outputPath);
        throw;
    }
}

void slideio::convertScene(ScenePtr scene,
                            ConverterParameters& parameters,
                            const std::string& outputPath)
{
    if(scene == nullptr) {
        RAISE_RUNTIME_ERROR << "Converter: invalid input scene!";
    }
    if(parameters.driver.empty()) {
        RAISE_RUNTIME_ERROR << "Converter: unspecified output format!";
    }
    std::string driver = parameters.driver;
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


