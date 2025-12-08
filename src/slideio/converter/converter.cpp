// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "convertersvstools.hpp"
#include "convertertools.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/converter/converter.hpp"
#include "slideio/converter/converterparameters.hpp"
#include "slideio/base/log.hpp"

#include <filesystem>

#include "convertertifftools.hpp"

using namespace slideio;
using namespace slideio::converter;

static void createTiff(TIFFKeeperPtr& file, const CVScenePtr& scene, ConverterParameters& parameters, ConverterCallback cb)
{
    auto containerType = parameters.getContainerType();
    if (containerType != TIFF_CONTAINER) {
        RAISE_RUNTIME_ERROR << "Expected TIFF container for SVS format. Received container type: " << (int)containerType;
    }
    std::shared_ptr<const TIFFContainerParameters> tiffParams = std::static_pointer_cast<const TIFFContainerParameters>(parameters.getContainerParameters());

    if (tiffParams->getNumZoomLevels() < 1) {
        RAISE_RUNTIME_ERROR << "Expected positive number of zoom levels. Received: " << tiffParams->getNumZoomLevels();
    }
    if (tiffParams->getTileHeight() <= 0 || tiffParams->getTileHeight() <= 0) {
        RAISE_RUNTIME_ERROR << "Expected not empty tile size. Received: "
            << tiffParams->getTileHeight() << "x" << tiffParams->getTileHeight();
    }
    if (!file->isValid()) {
        RAISE_RUNTIME_ERROR << "Received invalid tiff file handle!";
    }
    if (!scene) {
        RAISE_RUNTIME_ERROR << "Received invalid scene object!";
    }

     ConverterTools::checkContainerRequirements(scene, parameters);
     ConverterTools::checkEncodingRequirements(scene, parameters);
    
    int tileCount = 0;
    
    if (cb) {
        cv::Rect sceneRect = scene->getRect();
        cv::Size sceneSize = scene->getRect().size();
        if (parameters.getRect().valid()) {
            const auto& block = parameters.getRect();
            sceneSize.width = block.width;
            sceneSize.height = block.height;
        }
        for (int zoomLevel = 0; zoomLevel < tiffParams->getNumZoomLevels(); ++zoomLevel) {
            const cv::Size tileSize(tiffParams->getTileWidth(), tiffParams->getTileHeight());
            const cv::Size levelImageSize = ConverterTools::scaleSize(sceneSize, zoomLevel);
            const int sx = (levelImageSize.width - 1) / tileSize.width + 1;
            const int sy = (levelImageSize.height - 1) / tileSize.height + 1;
            tileCount += sx * sy;
        }
    }
    
    int percents = 0;
    int processedTiles = 0;
    auto lambda = [cb, tileCount, &processedTiles, &percents](int, int)
        {
            const int newPercents = (processedTiles * 100) / tileCount;
            processedTiles++;
            if (newPercents != percents) {
                cb(newPercents);
                percents = newPercents;
            }
        };
    
    std::string description;
    if (parameters.getFormat()==ImageFormat::SVS) {
        description = ConverterSVSTools::createDescription(scene, parameters);
	}

    for (int zoomLevel = 0; zoomLevel < tiffParams->getNumZoomLevels(); ++zoomLevel) {
        if (cb) {
            ConverterTiffTools::createZoomLevel(file, zoomLevel, description, scene, parameters, lambda);
        }
        else {
            ConverterTiffTools::createZoomLevel(file, zoomLevel, description, scene, parameters, nullptr);
        }
    }
    if (cb != nullptr && percents != 100) {
        cb(100);
    }
}

static void convertToTiff(CVScenePtr scene, ConverterParameters& params, const std::string& outputPath, ConverterCallback cb)
{
    if(params.getFormat() != ImageFormat::SVS && params.getFormat() != ImageFormat::OME_TIFF) {
        RAISE_RUNTIME_ERROR << "Incorrect parameter type for the output file";
    }
    try {
        if (params.getContainerType() != TIFF_CONTAINER) {
            RAISE_RUNTIME_ERROR << "Converter: SVS format supports only TIFF container!";
		}
        std::shared_ptr<TIFFContainerParameters> tiffParams = std::static_pointer_cast<TIFFContainerParameters>(params.getContainerParameters());
        TIFFKeeperPtr file(new TIFFKeeper(outputPath, false));
        if (tiffParams->getNumZoomLevels() <= 0) {
            const auto& rect = scene->getRect();
            const auto& block = params.getRect();
            int width = rect.width;
            int height = rect.height;
            if(block.valid()) {
                width = block.width;
                height = block.height;
            }
            tiffParams->setNumZoomLevels(ConverterTools::computeNumZoomLevels(width, height));
        }
        createTiff(file, scene, params, cb);
    }
    catch(std::exception&) {
#if defined(WIN32)
        std::filesystem::remove(Tools::toWstring(outputPath));
#else
        std::filesystem::remove(outputPath);
#endif
        throw;
    }
}

void slideio::converter::convertScene(ScenePtr scene,
                            ConverterParameters& parameters,
                            const std::string& outputPath,
                            ConverterCallback cb)
{
    if(scene == nullptr) {
        RAISE_RUNTIME_ERROR << "Converter: invalid input scene!";
    }
    if (parameters.getFormat() != ImageFormat::SVS && parameters.getFormat() != ImageFormat::OME_TIFF) {
        RAISE_RUNTIME_ERROR << "Converter: output format '" << (int)parameters.getFormat() << "' is not supported!";
    }
    if(parameters.getEncoding() != Compression::Jpeg
        && parameters.getEncoding() != Compression::Jpeg2000) {
        RAISE_RUNTIME_ERROR << "Unsupported compression type: " << (int)parameters.getEncoding();
    }
    if(std::filesystem::exists(outputPath)) {
        RAISE_RUNTIME_ERROR << "Converter: output file already exists.";
    }
    std::string sceneName = scene->getName();
    std::string filePath = scene->getFilePath();
    SLIDEIO_LOG(INFO) << "Convert a scene " << sceneName << " from file "
            << filePath << " to format: '" << (int)parameters.getFormat() << "'.";
    convertToTiff(scene->getCVScene(), parameters, outputPath, cb);
}


