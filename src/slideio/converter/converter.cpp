// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <boost/filesystem.hpp>
#include "convertersvstools.hpp"
#include "convertertools.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/converter/converter.hpp"
#include "slideio/converter/converterparameters.hpp"
#include "slideio/base/log.hpp"

using namespace slideio;


static void convertToSVS(CVScenePtr scene, ConverterParameters& params, const std::string& outputPath, ConverterCallback cb)
{
    if(params.getFormat() != ImageFormat::SVS) {
        RAISE_RUNTIME_ERROR << "Incorrect parameter type for the output file";
    }
    try {
        SVSConverterParameters& parameters = static_cast<SVSConverterParameters&>(params);
        TIFFKeeperPtr file(new TIFFKeeper(outputPath, false));
        if (parameters.getNumZoomLevels() <= 0) {
            const auto& rect = scene->getRect();
            const auto& block = params.getRect();
            int width = rect.width;
            int height = rect.height;
            if(block.valid()) {
                width = block.width;
                height = block.height;
            }
            parameters.setNumZoomLevels(ConverterTools::computeNumZoomLevels(width, height));
        }
        ConverterSVSTools::createSVS(file, scene, parameters, cb);
    }
    catch(std::exception&) {
#if defined(WIN32)
        boost::filesystem::remove(Tools::toWstring(outputPath));
#else
        boost::filesystem::remove(outputPath);
#endif
        throw;
    }
}

void slideio::convertScene(ScenePtr scene,
                            ConverterParameters& parameters,
                            const std::string& outputPath,
                            ConverterCallback cb)
{
    if(scene == nullptr) {
        RAISE_RUNTIME_ERROR << "Converter: invalid input scene!";
    }
    if (parameters.getFormat() != ImageFormat::SVS) {
        RAISE_RUNTIME_ERROR << "Converter: output format '" << (int)parameters.getFormat() << "' is not supported!";
    }
    SVSConverterParameters& svsParameters = (SVSConverterParameters&)parameters;
    if(svsParameters.getEncoding() != Compression::Jpeg
        && svsParameters.getEncoding() != Compression::Jpeg2000) {
        RAISE_RUNTIME_ERROR << "Unsupported compression type: " << (int)svsParameters.getEncoding();
    }
    if(boost::filesystem::exists(outputPath)) {
        RAISE_RUNTIME_ERROR << "Converter: output file already exists.";
    }
    std::string sceneName = scene->getName();
    std::string filePath = scene->getFilePath();
    SLIDEIO_LOG(INFO) << "Convert a scene " << sceneName << " from file "
            << filePath << " to format: '" << (int)parameters.getFormat() << "'.";
    convertToSVS(scene->getCVScene(), parameters, outputPath, cb);
}


