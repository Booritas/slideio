// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "vsitools.hpp"
#include "vsislide.hpp"

using namespace slideio;

DataType vsi::VSITools::toSlideioPixelType(uint32_t vsiPixelType)
{
    switch(vsiPixelType)
    {
      case vsi::PixelType::CHAR: return DataType::DT_Int8;
      case vsi::PixelType::UCHAR: return DataType::DT_Byte;
      case vsi::PixelType::SHORT: return DataType::DT_Int16;
	  case vsi::PixelType::USHORT: return DataType::DT_UInt16;
	  case vsi::PixelType::INT: return DataType::DT_Int32;
	  case vsi::PixelType::UINT: return DataType::DT_UInt32;
	  case vsi::PixelType::INT64: return DataType::DT_Int64;
	  case vsi::PixelType::UINT64: return DataType::DT_UInt64;
	  case vsi::PixelType::FLOAT: return DataType::DT_Float32;
	  case vsi::PixelType::DOUBLE: return DataType::DT_Float64;
    }
	RAISE_RUNTIME_ERROR << "VSI Driver: Unsupported pixel type: " << vsiPixelType;
}

slideio::Compression vsi::VSITools::toSlideioCompression(vsi::Compression format)
{
	switch(format) {
	case vsi::Compression::RAW: return slideio::Compression::Uncompressed;
	case vsi::Compression::JPEG: return slideio::Compression::Jpeg;
	case vsi::Compression::JPEG_2000: return slideio::Compression::Jpeg2000;
	case vsi::Compression::JPEG_LOSSLESS: return slideio::Compression::JpegLossless;
	case vsi::Compression::PNG: return slideio::Compression::Png;
	case vsi::Compression::BMP: return slideio::Compression::BMP;
	}
	RAISE_RUNTIME_ERROR << "VSI Driver: Unsupported compression type: " << static_cast<uint32_t>(format);
}
