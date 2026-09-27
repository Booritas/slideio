// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/svs/svs_api_def.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include <opencv2/core.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <optional>

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
        // The scan time an Aperio description states, in seconds since
        // 1970-01-01T00:00:00Z, or nullopt where it cannot be read.
        //
        // Aperio splits it across two properties, "Date = 12/29/09" and
        // "Time = 09:59:15" -- month first, and a year that may be two digits;
        // 00-68 is 2000-2068 and 69-99 is 1969-1999, the window strptime's %y
        // uses. Java's SimpleDateFormat, which Bio-Formats parses this with,
        // slides its window with the current date instead, so the two disagree
        // about a year far enough out and only this one answers the same way
        // next decade. A "Time Zone = GMT-05:00" property is applied when the
        // file states one; without it the text is read as UTC, because assuming
        // the reader's zone would make one file yield different instants on
        // different machines. Static and public so the formats can be tested
        // directly -- no corpus file states a zone.
        static std::optional<int64_t> aperioDateTimeToEpochSeconds(const std::string& date,
                                                                   const std::string& time,
                                                                   const std::string& zone);
        // The significant bits an Aperio image description states, or 0 where
        // it states none.
        //
        // Aperio writes "Acquisition Bit Depth = 10" for a camera digitising
        // narrower than the sample it is stored in. It is a property of the same
        // header the scan time comes from, not a TIFF tag, which is why a survey
        // of the tags alone concluded SVS states nothing.
        static int significantBitsFromDescription(const std::string& description);
        // The scan time an Aperio image description states, in seconds since
        // the Unix epoch, or 0 where it states none or none that can be read.
        // Reads the Date, Time and Time Zone properties of the description.
        static int64_t acquisitionTimeFromDescription(const std::string& description);
        // Serializes a TiffDirectory (and its subdirectories) to JSON.
        static nlohmann::json tiffDirectoryToJson(const TiffDirectory& dir);
    };
}
