// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/lif/lif_api_def.hpp"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace slideio
{
    namespace lif
    {
        /// One DimensionDescription.
        struct Dimension
        {
            int dimId = 0;
            int64_t count = 0;
            int64_t bytesInc = 0;
            double origin = 0;
            double length = 0;
            std::string unit;
        };

        /// One ChannelDescription.
        struct Channel
        {
            int dataType = -1;          // -1: attribute absent
            int resolution = 0;         // significant bits
            int64_t bytesInc = 0;
            int channelTag = -1;        // -1: absent
            /// UserDefName of the WFC scanner settings. No file of the test corpus
            /// carries it (spec 6.1), so it is not parsed and stays empty.
            std::string name;
            std::vector<std::pair<std::string, std::string>> attributes;   // every attribute, verbatim
        };

        /// What the XML says about one image.
        struct ImageDescription
        {
            std::string path;            // "Nest/CZMT": Element names, experiment omitted
            std::string memoryId;
            int64_t declaredSize = -1;   // Memory/@Size, -1 if absent
            std::vector<Dimension> dimensions;   // document order
            std::vector<Channel> channels;       // document order
            std::vector<uint64_t> timestampTicks;
            double delayTimeSeconds = 0;         // scanner nDelayTime_s, 0 if absent
            double magnification = 0;            // HardwareSetting ATL*SettingDefinition/@Magnification
            /// One entry per TileScanInfo/Tile: (field index in document order,
            /// "FieldX=..;FieldY=..;PosX=..;PosY=..", attributes verbatim as written).
            std::vector<std::pair<std::string, std::string>> tileFieldAttributes;
            std::string rawXml;          // owning Element without child Elements
        };

        /// Every image of the XML description with its own memory block id, in
        /// document order (spec 4.3). Throws RuntimeError "LIF: ..." on malformed XML.
        SLIDEIO_LIF_EXPORTS std::vector<ImageDescription> parseImages(const std::string& xml);
    }
}
