// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_converter_HPP
#define OPENCV_slideio_converter_HPP

#include <map>

#include "slideio/converter/converter_def.hpp"
#include "slideio/core/cvstructs.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/slideio/scene.hpp"

const std::string DRIVER = "DRIVER";

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    struct ConverterParameters
    {
        ConverterParameters(ImageTools::EncodeParameters* encodeParameters) :
            numZoomLevels(0),
            tileWidth(256),
            tileHeight(256),
            encoding(encodeParameters) {
        }
        ImageTools::EncodeParameters* encoding;
        int numZoomLevels;
        std::string driver;
        int tileWidth;
        int tileHeight;
    };
   void SLIDEIO_CONVERTER_EXPORTS convertScene(std::shared_ptr<slideio::Scene> inputScene,
                                                ConverterParameters& parameters,
                                                const std::string& outputPath);
}

#endif