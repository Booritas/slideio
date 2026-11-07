// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/mrxs/mrxs_api_def.hpp"
#include "slideio/core/cvslide.hpp"
#include <memory>
#include <string>

namespace slideio
{
    class SLIDEIO_MRXS_EXPORTS MRXSSlide : public CVSlide
    {
    public:
        /**@brief opens the bundle an entry path names (UTF-8).*/
        static std::shared_ptr<MRXSSlide> openFile(const std::string& filePath);
    };
}
