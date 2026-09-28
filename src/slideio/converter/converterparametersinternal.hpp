// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/converter/converter_def.hpp"
#include <memory>

namespace slideio
{
    class CVScene;

    namespace converter
    {
        class ConverterParameters;

        // Fills in the parameters the caller left undefined -- the rect, the
        // channel, slice and frame ranges, and the zoom level count -- from the
        // scene being converted.
        //
        // A free function in an internal header rather than a member of
        // ConverterParameters, because ConverterParameters is a public header
        // and CVScene is not: as a member it put an internal type in the public
        // class's signature, so no consumer of the package could call it, while
        // every consumer still had to read a forward declaration of a class
        // whose definition they were never given. It needs no privileged access
        // -- everything it touches is reachable through the public accessors.
        SLIDEIO_CONVERTER_EXPORTS void updateNotDefinedParameters(
            ConverterParameters& parameters, const std::shared_ptr<CVScene>& scene);
    }
}
