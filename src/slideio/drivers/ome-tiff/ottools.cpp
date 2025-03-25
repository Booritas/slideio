// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/ome-tiff/ottools.hpp"

using namespace slideio;
using namespace slideio::ometiff;

DataType OTTools::stringToDataType(const std::string& type) {
	if (type == "int8") {
		return DataType::DT_Byte;
	}
	if (type == "uint8") {
		return DataType::DT_Byte;
	}
	if (type == "uint16") {
		return DataType::DT_UInt16;
	}
	if (type == "uint32") {
		return DataType::DT_UInt32;
	}
	if (type == "uint64") {
		return DataType::DT_UInt64;
	}
	if (type == "int8") {
		return DataType::DT_Int8;
	}
	if (type == "int16") {
		return DataType::DT_Int16;
	}
	if (type == "int32") {
		return DataType::DT_Int32;
	}
	if (type == "int64") {
		return DataType::DT_Int64;
	}
	if (type == "float") {
		return DataType::DT_Float32;
	}
	if (type == "double") {
		return DataType::DT_Float64;
	}
	return DataType::DT_Unknown;
}
