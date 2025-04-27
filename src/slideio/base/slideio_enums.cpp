// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include <string>
#include <ostream>
#include "slideio/base/slideio_enums.hpp"

std::string slideio::compressionToString(slideio::Compression compression) {
    switch (compression) {
    case Compression::Unknown: return "Unknown";
    case Compression::Uncompressed: return "Uncompressed";
    case Compression::Jpeg: return "Jpeg";
    case Compression::JpegXR: return "JpegXR";
    case Compression::Png: return "Png";
    case Compression::Jpeg2000: return "Jpeg2000";
    case Compression::LZW: return "LZW";
    case Compression::HuffmanRL: return "HuffmanRL";
    case Compression::CCITT_T4: return "CCITT_T4";
    case Compression::CCITT_T6: return "CCITT_T6";
    case Compression::JpegOld: return "JpegOld";
    case Compression::Zlib: return "Zlib";
    case Compression::JBIG85: return "JBIG85";
    case Compression::JBIG43: return "JBIG43";
    case Compression::NextRLE: return "NextRLE";
    case Compression::PackBits: return "PackBits";
    case Compression::ThunderScanRLE: return "ThunderScanRLE";
    case Compression::RasterPadding: return "RasterPadding";
    case Compression::RLE_LW: return "RLE_LW";
    case Compression::RLE_HC: return "RLE_HC";
    case Compression::RLE_BL: return "RLE_BL";
    case Compression::PKZIP: return "PKZIP";
    case Compression::KodakDCS: return "KodakDCS";
    case Compression::JBIG: return "JBIG";
    case Compression::NikonNEF: return "NikonNEF";
    case Compression::JBIG2: return "JBIG2";
    case Compression::GIF: return "GIF";
    case Compression::BIGGIF: return "BIGGIF";
    case Compression::RLE: return "RLE";
    }
    return "Unknown";
}

std::ostream& slideio::operator<<(std::ostream& os, slideio::Compression compression) {
    os << slideio::compressionToString(compression);
    return os;
}

std::ostream& slideio::operator << (std::ostream& os, const slideio::DataType& dt) {
    switch (dt) {
    case slideio::DataType::DT_Byte: os << "DT_Byte";  break;
    case slideio::DataType::DT_Int8: os << "DT_Int8"; break;
    case slideio::DataType::DT_Int16: os << "DT_Int16"; break;
    case slideio::DataType::DT_Float16: os << "DT_Float16"; break;
    case slideio::DataType::DT_Int32: os << "DT_Int32"; break;
    case slideio::DataType::DT_Float32: os << "DT_Float32"; break;
    case slideio::DataType::DT_Float64: os << "DT_Float64"; break;
    case slideio::DataType::DT_UInt16: os << "DT_UInt16"; break;
    case slideio::DataType::DT_Unknown: os << "DT_Unknown"; break;
    case slideio::DataType::DT_None: os << "DT_None"; break;
    default: os << (int)dt;
    }
    return os;
}

std::ostream& slideio::operator << (std::ostream& os, const slideio::MetadataFormat& mt) {
	switch (mt) {
	case slideio::MetadataFormat::None: os << "None"; break;
	case slideio::MetadataFormat::Unknown: os << "Unknown"; break;
	case slideio::MetadataFormat::Text: os << "TEXT"; break;
	case slideio::MetadataFormat::XML: os << "XML"; break;
	case slideio::MetadataFormat::JSON: os << "JSON"; break;
	}
	return os;
}


