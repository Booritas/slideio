// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/svs/svs_api_def.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include <opencv2/core.hpp>
#include <nlohmann/json.hpp>
#include <string>

namespace slideio
{
    class SLIDEIO_SVS_EXPORTS SVSTools
    {
    public:
        // Extracts magnification value from image information string
        static int extractMagnifiation(const std::string& description);
        // Extracts resolution value from image information string
        static double extractResolution(const std::string& description);
        // The magnification of the base zoom level, derived from the description of a
        // philips zoom level directory ("level=1 mag=20 quality=80" -> 40). Philips names
        // the magnification of every level but the base, whose directory carries the xml
        // metadata instead; a level covers 2^-level of the base, so the base is
        // mag * 2^level. Returns 0 if the description names no usable magnification.
        static double extractPhilipsMagnification(const std::string& description);
        // The zoom level a philips level directory's description names
        // ("level=1 mag=20 quality=80" -> 1), or -1 if it names none. The base level's
        // directory carries the xml metadata instead and so names none.
        static int extractPhilipsLevelNumber(const std::string& description);
        // Parses an Aperio-format metadata string into a structured JSON tree.
        // Header lines (before the first '|') become "application" and "image";
        // subsequent "name = value" tokens become entries under "properties".
        static nlohmann::json parseAperioMetadata(const std::string& description);
        // Serializes a TiffDirectory (and its subdirectories) to JSON.
        static nlohmann::json tiffDirectoryToJson(const TiffDirectory& dir);
    };
}
