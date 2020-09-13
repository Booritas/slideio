#pragma once
#include <codecvt>
#include <string>
#include <pole/polepp.hpp>

namespace zviutils
{
	typedef  enum tagVARENUM
	{
		VT_EMPTY = 0x0000,
		VT_NULL = 0x0001,
		VT_I2 = 0x0002,
		VT_I4 = 0x0003,
		VT_R4 = 0x0004,
		VT_R8 = 0x0005,
		VT_CY = 0x0006,
		VT_DATE = 0x0007,
		VT_BSTR = 0x0008,
		VT_DISPATCH = 0x0009,
		VT_ERROR = 0x000A,
		VT_BOOL = 0x000B,
		VT_VARIANT = 0x000C,
		VT_UNKNOWN = 0x000D,
		VT_DECIMAL = 0x000E,
		VT_I1 = 0x0010,
		VT_UI1 = 0x0011,
		VT_UI2 = 0x0012,
		VT_UI4 = 0x0013,
		VT_I8 = 0x0014,
		VT_UI8 = 0x0015,
		VT_INT = 0x0016,
		VT_UINT = 0x0017,
		VT_VOID = 0x0018,
		VT_HRESULT = 0x0019,
		VT_PTR = 0x001A,
		VT_SAFEARRAY = 0x001B,
		VT_CARRAY = 0x001C,
		VT_USERDEFINED = 0x001D,
		VT_LPSTR = 0x001E,
		VT_LPWSTR = 0x001F,
		VT_RECORD = 0x0024,
		VT_INT_PTR = 0x0025,
		VT_UINT_PTR = 0x0026,
		VT_ARRAY = 0x2000,
		VT_BYREF = 0x4000
	} VARENUM;

	void skipItem(ole::basic_stream& stream)
	{
		uint16_t type;
		stream.read((char*)&type, sizeof(type));
		uint32_t offset = 0;
		switch (type)
	    {
		case VT_EMPTY:
		case VT_NULL:
			break;
		case VT_I1:
		case VT_UI1:
			offset = 1;
			break;
		case VT_I2:
		case VT_UI2:
		case VT_BOOL:
			offset = 2;
			break;
		case VT_I4:
		case VT_INT:
		case VT_UI4:
		case VT_UINT:
			offset = 4;
			break;
		case VT_I8:
		case VT_UI8:
		case VT_DATE:
		case VT_R4:
			offset = 4;
			break;
		case VT_R8:
			offset = 8;
			break;
		case VT_BSTR:
		case VT_ARRAY:
			stream.read((char*)&offset, 4);
			break;
		case VT_DISPATCH:
		case VT_UNKNOWN:
			offset = 16;
			break;
		}
		stream.seek(offset, std::ios::cur);
	}

	void skipItems(ole::basic_stream& stream, int count)
	{
	    for(int item=0; item<count; item++)
	    {
			skipItem(stream);
	    }
	}

	int32_t readOleInt(ole::basic_stream& stream)
	{
		uint16_t type(0);
		stream.read((char*)&type, sizeof(type));
		if(type!=VT_I4 && type!=VT_INT)
		{
			std::string error = "Unexpected data type reading of compound stream. Expected integer. Received:";
			error += std::to_string(type);
			throw std::runtime_error(error);
		}
		int32_t value;
		stream.read((char*)&value, sizeof(value));
		return value;
	}

	double readOleDouble(ole::basic_stream& stream)
	{
		uint16_t type(0);
		stream.read((char*)&type, sizeof(type));
		if (type != VT_R8)
		{
			std::string error = "Unexpected data type reading of compound stream. Expected VT_R8. Received:";
			error += std::to_string(type);
			throw std::runtime_error(error);
		}
		double value;
		stream.read((char*)&value, sizeof(value));
		return value;
	}

    std::string readOleString(ole::basic_stream& stream)
	{
		uint16_t type(0);
		stream.read((char*)&type, sizeof(type));
		if (type != VT_BSTR)
		{
			std::string error = "Unexpected data type reading of compound stream. Expected string. Received:";
			error += std::to_string(type);
			throw std::runtime_error(error);
		}
		int32_t string_length;
		stream.read((char*)&string_length, sizeof(string_length));
		std::string value;
		if (string_length>0)
		{
			std::vector<char> buffer(string_length,0);
			stream.read((char*)buffer.data(), string_length);
            std::u16string src((char16_t *)buffer.data());
            std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
            value = convert.to_bytes(src);
		}
		return value;
	}
}
