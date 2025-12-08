// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_convertersvstools_HPP
#define OPENCV_slideio_convertersvstools_HPP

#include "slideio/converter/converter_def.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/converter/convertercallback.hpp"

namespace slideio
{
    class CVScene;
    namespace converter
    {
        class TIFFConverterParameters;
        class ConverterParameters;
        class SLIDEIO_CONVERTER_EXPORTS ConverterSVSTools
        {
        public:
            static void checkSVSRequirements(const std::shared_ptr<CVScene>& scene, const ConverterParameters& parameters);
            static std::string createDescription(const std::shared_ptr<CVScene>& scene, const ConverterParameters& parameters);
        };
    }
}
#endif