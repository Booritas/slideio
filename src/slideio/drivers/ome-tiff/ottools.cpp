// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/ome-tiff/ottools.hpp"

#include "slideio/base/log.hpp"

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
	else if (unit == "\xC2\xB5m") { // µm (micrometer)
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
	else if (unit == "\xC3\x85") { // Å (Angstrom)
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
