// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/ome-tiff/ottools.hpp"
#include <cstdio>
#include <map>
#include <tinyxml2.h>
#include "slideio/core/log.hpp"

#include "slideio/core/log.hpp"

using namespace slideio;
using namespace slideio::ometiff;

DataType OTTools::stringToDataType(const std::string& type) {
    // normalize input to lowercase for case-insensitive comparison
    std::string caseInsensitiveType = type;
    std::transform(caseInsensitiveType.begin(), caseInsensitiveType.end(), caseInsensitiveType.begin(), [](unsigned char c){ return std::tolower(c); });
	if (caseInsensitiveType == "int8") {
		return DataType::DT_Int8;
	}
	if (caseInsensitiveType == "uint8") {
		return DataType::DT_Byte;
	}
	if (caseInsensitiveType == "uint16") {
		return DataType::DT_UInt16;
	}
	if (caseInsensitiveType == "uint32") {
		return DataType::DT_UInt32;
	}
	if (caseInsensitiveType == "uint64") {
		return DataType::DT_UInt64;
	}
	if (caseInsensitiveType == "int8") {
		return DataType::DT_Int8;
	}
	if (caseInsensitiveType == "int16") {
		return DataType::DT_Int16;
	}
	if (caseInsensitiveType == "int32") {
		return DataType::DT_Int32;
	}
	if (caseInsensitiveType == "int64") {
		return DataType::DT_Int64;
	}
	if (caseInsensitiveType == "float") {
		return DataType::DT_Float32;
	}
	if (caseInsensitiveType == "double") {
		return DataType::DT_Float64;
	}
	return DataType::DT_Unknown;
}

double OTTools::convertToMeters(double value, const std::string& unit) {
	if (unit == "m") {
		return value;
	}
	else if (unit == "\xC2\xB5m") { // �m (micrometer)
		return value * 1.e-6; // Micrometer
	}
	else if (unit == "mm") {
		return value * 0.001;
	}
	else if (unit == "um") {
		return value * 0.000001;
	}
	else if (unit == "nm") {
		return value * 0.000000001;
	}
	else if (unit == "cm") {
		return value * 0.01;
	}
	else if (unit == "km") {
		return value * 1000.0;
	}
	else if (unit == "Ym") {
		return value * 1e-24; // Yoctometer
	}
	else if (unit == "Zm") {
		return value * 1e-21; // Zeptometer
	}
	else if (unit == "am") {
		return value * 1e-18; // Attometer
	}
	else if (unit == "fm") {
		return value * 1e-15; // Femtometer
	}
	else if (unit == "pm") {
		return value * 1e-12; // Picometer
	}
	else if (unit == "Em") {
		return value * 1e-18; // Exameter
	}
	else if (unit == "Pm") {
		return value * 1e-15; // Petameter
	}
	else if (unit == "Tm") {
		return value * 1e-12; // Terameter
	}
	else if (unit == "Gm") {
		return value * 0.000000001; // Gigameter
	}
	else if (unit == "Mm") {
		return value * 0.001; // Megameter
	}
	else if (unit == "dm") {
		return value * 0.1; // Decimeter
	}
	else if (unit == "hm") {
		return value * 10.0; // Hectometer
	}
	else if (unit == "dam") {
		return value * 0.1; // Dekameter
	}
	else if (unit == "\xC3\x85") { // � (Angstrom)
		return value * 0.0000000001; // Angstrom
	}
	else if (unit == "thou") {
		return value * 0.0000254; // Thou (thousandth of an inch)
	}
	else if (unit == "mil") {
		return value * 0.0000254; // Mil (thousandth of an inch)
	}
	else if (unit == "in") {
		return value * 0.0254; // Inch
	}
	else if (unit == "ft") {
		return value * 0.3048; // Foot
	}
	else if (unit == "yd") {
		return value * 0.9144; // Yard
	}
	else if (unit == "mi") {
		return value * 1609.34; // Mile
	}
	else if (unit == "ua") {
		return value * 149597870700.0; // Astronomical Unit (AU) in meters
	}
	else if (unit == "ly") {
		return value * 9.4607e15; // Light Year in meters
	}
	else if (unit == "pc") {
		return value * 3.0857e16; // Parsec in meters
	}
	else if (unit == "pt") {
		return value * 0.000352777778; // Point (1/72 inch) in meters
	}
	else if (unit == "pc") {
		return value * 0.000352777778; // Pica (12 points) in meters
	}
	else if (unit == "pixel") {
		return value; 
	}
	SLIDEIO_LOG(WARNING) << "OTTools::convertToMeters: unsupported unit: " << unit;
	return value; 
}

double OTTools::convertToSeconds(double tResolution, const std::string& units) {
	if (units == "s") {
		return tResolution;
	}
	else if (units == "ms") {
		return tResolution * 0.001;
	}
	else if (units == "us") {
		return tResolution * 0.000001;
	}
	else if (units == "ns") {
		return tResolution * 0.000000001;
	}
	else if (units == "ps") {
		return tResolution * 0.000000000001;
	}
	else if (units == "fs") {
		return tResolution * 0.000000000000001;
	}
	else if (units == "as") {
		return tResolution * 0.000000000000000001; // Attosecond
	}
	else if (units == "zs") {
		return tResolution * 0.000000000000000000001; // Zeptosecond
	}
	else if (units == "ys") {
		return tResolution * 0.000000000000000000000001; // Yoctosecond
	}
	else if (units == "min") {
		return tResolution * 60.0; // Minute
	}
	else if (units == "h") {
		return tResolution * 3600.0; // Hour
	}
	else if (units == "d") {
		return tResolution * 86400.0; // Day
	}
	SLIDEIO_LOG(WARNING) << "OTTools::convertToSeconds: unsupported unit: " << units;
    return tResolution;
}

std::optional<double> OTTools::timeUnitToSeconds(const std::string& units) {
    static const std::map<std::string, double> scales = {
        {"s", 1.0},
        {"ms", 1e-3},
        {"us", 1e-6},
        {"\xc2\xb5s", 1e-6},   // UTF-8 micro sign, which is what OME writes
        {"ns", 1e-9},
        {"ps", 1e-12},
        {"fs", 1e-15},
        {"as", 1e-18},
        {"zs", 1e-21},
        {"ys", 1e-24},
        {"min", 60.0},
        {"h", 3600.0},
        {"d", 86400.0},
    };
    const auto it = scales.find(units);
    if (it == scales.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<int64_t> OTTools::parseAcquisitionDate(const std::string& text) {
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    // xsd:dateTime. Anything after the seconds -- fractional digits, "Z", an
    // offset -- is read separately below; %n reports how far sscanf got.
    int consumed = 0;
    if (std::sscanf(text.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d%n",
                    &year, &month, &day, &hour, &minute, &second, &consumed) != 6) {
        return std::nullopt;
    }
    if (month < 1 || month > 12 || day < 1 || day > 31
        || hour > 23 || minute > 59 || second > 60) {
        return std::nullopt;
    }
    static const int monthLengths[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    const int daysInMonth = monthLengths[month - 1] + ((leap && month == 2) ? 1 : 0);
    if (day > daysInMonth) {
        return std::nullopt;
    }

    // Days from the civil date, after Howard Hinnant: no timegm, which is not
    // portable, and no mktime, which would drag in the local timezone.
    int y = year;
    y -= month <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const int64_t days = static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;

    int64_t epoch = days * 86400 + hour * 3600LL + minute * 60LL + second;

    // A trailing offset, if the file states one. Fractional seconds are skipped
    // rather than rounded: the getter reports whole seconds.
    const char* rest = text.c_str() + consumed;
    if (*rest == '.') {
        ++rest;
        while (*rest >= '0' && *rest <= '9') {
            ++rest;
        }
    }
    if (*rest == '+' || *rest == '-') {
        int offsetHours = 0, offsetMinutes = 0;
        if (std::sscanf(rest + 1, "%2d:%2d", &offsetHours, &offsetMinutes) == 2) {
            const int64_t offset = offsetHours * 3600LL + offsetMinutes * 60LL;
            epoch += (*rest == '+') ? -offset : offset;
        }
    }
    return epoch;
}

std::optional<std::vector<double>> OTTools::collectPlaneTimestamps(
    const tinyxml2::XMLElement* pixels, int numTFrames, int numChannels, int numZSlices) {
    if (pixels == nullptr || numTFrames <= 0 || numChannels <= 0 || numZSlices <= 0) {
        return std::nullopt;
    }
    const size_t planeCount =
        static_cast<size_t>(numTFrames) * numChannels * numZSlices;
    std::vector<double> timestamps(planeCount, 0.);
    std::vector<bool> stated(planeCount, false);
    size_t statedCount = 0;

    for (const tinyxml2::XMLElement* plane = pixels->FirstChildElement("Plane");
         plane != nullptr;
         plane = plane->NextSiblingElement("Plane")) {
        double delta = 0.;
        if (plane->QueryDoubleAttribute("DeltaT", &delta) != tinyxml2::XML_SUCCESS) {
            // A plane that states no time. The series cannot be completed.
            return std::nullopt;
        }
        // DeltaTUnit defaults to seconds when the file does not state one.
        const char* unit = plane->Attribute("DeltaTUnit");
        double scale = 1.;
        if (unit != nullptr) {
            const auto known = timeUnitToSeconds(unit);
            if (!known) {
                SLIDEIO_LOG(WARNING) << "OTTools: unreadable DeltaTUnit '" << unit
                    << "'; discarding the plane timestamps";
                return std::nullopt;
            }
            scale = *known;
        }
        const int t = plane->IntAttribute("TheT", 0);
        const int c = plane->IntAttribute("TheC", 0);
        const int z = plane->IntAttribute("TheZ", 0);
        if (t < 0 || t >= numTFrames || c < 0 || c >= numChannels
            || z < 0 || z >= numZSlices) {
            SLIDEIO_LOG(WARNING) << "OTTools: plane (" << t << "," << c << "," << z
                << ") lies outside the scene; discarding the plane timestamps";
            return std::nullopt;
        }
        const size_t index =
            (static_cast<size_t>(t) * numZSlices + z) * numChannels + c;
        if (!stated[index]) {
            ++statedCount;
        }
        stated[index] = true;
        timestamps[index] = delta * scale;
    }

    if (statedCount != planeCount) {
        // Either the file states no planes at all, which is normal, or it states
        // some but not all, which would leave a plane reporting a time it has not
        // got. Both mean there is no series to report.
        return std::nullopt;
    }
    return timestamps;
}

double OTTools::readZSliceResolution(const tinyxml2::XMLElement* pixels) {
    if (pixels == nullptr) {
        return 0.;
    }
    const double size = pixels->DoubleAttribute("PhysicalSizeZ", 0.0);
    const char* unit = pixels->Attribute("PhysicalSizeZUnit");
    if (unit == nullptr) {
        return size;
    }
    return convertToMeters(size, unit);
}

double OTTools::readTFrameResolution(const tinyxml2::XMLElement* pixels) {
    if (pixels == nullptr) {
        return 0.;
    }
    const double size = pixels->DoubleAttribute("PhysicalSizeT", 0.0);
    const char* unit = pixels->Attribute("PhysicalSizeTUnit");
    if (unit == nullptr) {
        // The OME default for PhysicalSizeTUnit is seconds.
        return size;
    }
    const auto scale = timeUnitToSeconds(unit);
    if (!scale) {
        SLIDEIO_LOG(WARNING) << "OTTools: unreadable PhysicalSizeTUnit '" << unit
            << "'; reporting no time-frame resolution";
        return 0.;
    }
    return size * (*scale);
}
