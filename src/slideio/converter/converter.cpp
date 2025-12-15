// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/tools/tools.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/converter/converter.hpp"
#include "slideio/converter/converterparameters.hpp"
#include "slideio/base/log.hpp"
#include "tiffconverter.hpp"

#include <filesystem>

using namespace slideio;
using namespace slideio::converter;

void converter::convertScene(std::shared_ptr<Scene> scene, ConverterParameters& parameters, const std::string& outputPath, ConverterCallback cb)
{
    if(scene == nullptr) {
        RAISE_RUNTIME_ERROR << "Converter: invalid input scene!";
    }
    if (parameters.getFormat() != ImageFormat::SVS && parameters.getFormat() != ImageFormat::OME_TIFF) {
        RAISE_RUNTIME_ERROR << "Converter: output format '" << (int)parameters.getFormat() << "' is not supported!";
    }
    if(parameters.getEncoding() != Compression::Jpeg
        && parameters.getEncoding() != Compression::Jpeg2000) {
        RAISE_RUNTIME_ERROR << "Unsupported compression type: " << parameters.getEncoding();
    }
    if(std::filesystem::exists(outputPath)) {
        RAISE_RUNTIME_ERROR << "Converter: output file already exists.";
    }
    std::string sceneName = scene->getName();
    std::string filePath = scene->getFilePath();
    SLIDEIO_LOG(INFO) << "Convert a scene " << sceneName << " from file "
        << filePath << " to format: '" << (int)parameters.getFormat() << "'.";
    if (parameters.getFormat() != ImageFormat::SVS && parameters.getFormat() != ImageFormat::OME_TIFF) {
        RAISE_RUNTIME_ERROR << "Incorrect parameter type for the output file";
    }
    try {
        TiffConverter structure;
        structure.createFileLayout(scene->getCVScene(), parameters);
		structure.createTiff(outputPath, cb);
    }
    catch (std::exception&) {
#if defined(WIN32)
        std::filesystem::remove(Tools::toWstring(outputPath));
#else
        std::filesystem::remove(outputPath);
#endif
        throw;
    }
}


