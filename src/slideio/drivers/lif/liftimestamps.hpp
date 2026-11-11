// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/lif/lif_api_def.hpp"
#include <cstdint>
#include <vector>

namespace tinyxml2
{
    class XMLElement;
}

namespace slideio
{
    namespace lif
    {
        /// Timestamps of an image, in FILETIME ticks (100 ns), in storage order.
        ///
        /// The argument is the image's `Data/Image` element, NOT its owning
        /// `Element`: both encodings of the list (`TimeStampList` with a hex
        /// text body, and `TimeStampList` with `TimeStamp HighInteger/LowInteger`
        /// children) are direct children of `Image`, beside `ImageDescription`
        /// (see auxfiles/lif-fixtures/generate.py, fixture `timestamps`).
        /// Returns an empty vector when the image carries none.
        SLIDEIO_LIF_EXPORTS std::vector<uint64_t> parseTimestampTicks(const tinyxml2::XMLElement* image);
    }
}
