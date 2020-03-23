// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_svstools_HPP
#define OPENCV_slideio_svstools_HPP

#include "slideio/slideio_def.hpp"
#include <opencv2/core.hpp>
#include <string>

namespace slideio
{
    class SLIDEIO_EXPORTS SVSTools
    {
    public:
        static int extractMagnifiation(const std::string& description);
    };
}

#endif