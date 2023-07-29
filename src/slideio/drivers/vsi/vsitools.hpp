// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <cstdint>

#include "etsfile.hpp"
#include "slideio/base/slideio_enums.hpp"

namespace slideio
{
    namespace vsi
    {
        class VSITools
        {
        public:
            static DataType toSlideioPixelType(uint32_t vsiPixelType);
            static slideio::Compression toSlideioCompression(vsi::Compression format);
        };
        
    }
}
