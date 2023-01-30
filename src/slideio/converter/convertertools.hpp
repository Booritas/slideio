// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_convertertools_HPP
#define OPENCV_slideio_convertertools_HPP

#include "slideio/converter/converter_def.hpp"

namespace slideio
{
    class SLIDEIO_CONVERTER_EXPORTS ConverterTools
    {
    public:
        static int computeNumZoomLevels(int width, int height);
    };
}

#endif