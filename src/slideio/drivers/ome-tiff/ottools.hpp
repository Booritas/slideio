// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/ome-tiff/ot_api_def.hpp"
#include <opencv2/core.hpp>
#include <string>

#include "slideio/base/slideio_enums.hpp"

namespace slideio
{
    namespace ometiff
    {
        class SLIDEIO_OMETIFF_EXPORTS OTTools
        {
        public:
			static DataType stringToDataType(const std::string& type);
        };
    }
}
