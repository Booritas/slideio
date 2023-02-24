// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_convertersvstools_HPP
#define OPENCV_slideio_convertersvstools_HPP

#include "slideio/converter/converter_def.hpp"
#include "slideio/core/cvscene.hpp"

namespace slideio
{
    class SLIDEIO_CONVERTER_EXPORTS ConverterSVSTools
    {
    public:
        static void checkSVSRequirements(const CVScenePtr& scene);
        static std::string createDescription(const CVScenePtr& scene);
    };
}
#endif