// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_convertertifftools_HPP
#define OPENCV_slideio_convertertifftools_HPP
#include "slideio/converter/converter_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"

namespace slideio
{
    namespace converter
    {
        class ConverterParameters;
        class SLIDEIO_CONVERTER_EXPORTS ConverterTiffTools
        {
        public:
            static void createZoomLevel(TIFFKeeperPtr& file, int zoomLevel, const std::string& description, const std::shared_ptr<CVScene>& scene, converter::ConverterParameters& parameters, const std::function<void(int, int)>& cb = nullptr);
        };
    }

}
#endif