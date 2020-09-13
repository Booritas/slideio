// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "zviutils.hpp"

void ZVIUtils::skipItem(ole::basic_stream& stream)
{
   uint16_t type;
   stream.read((char*)&type, sizeof(type));
   uint32_t offset = 0;
   switch(type)
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

void ZVIUtils::skipItems(ole::basic_stream& stream, int count)
{
   for(int item = 0; item < count; item++)
   {
      skipItem(stream);
   }
}

int32_t ZVIUtils::readIntItem(ole::basic_stream& stream)
{
   uint16_t type(0);
   stream.read((char*)&type, sizeof(type));
   if(type != VT_I4 && type != VT_INT)
   {
      std::string error =
         "Unexpected data type reading of compound stream. Expected integer. Received:";
      error += std::to_string(type);
      throw std::runtime_error(error);
   }
   int32_t value;
   stream.read((char*)&value, sizeof(value));
   return value;
}

double ZVIUtils::readDoubleItem(ole::basic_stream& stream)
{
   uint16_t type(0);
   stream.read((char*)&type, sizeof(type));
   if(type != VT_R8)
   {
      std::string error =
         "Unexpected data type reading of compound stream. Expected VT_R8. Received:";
      error += std::to_string(type);
      throw std::runtime_error(error);
   }
   double value;
   stream.read((char*)&value, sizeof(value));
   return value;
}

std::string ZVIUtils::readStringItem(ole::basic_stream& stream)
{
   uint16_t type(0);
   stream.read((char*)&type, sizeof(type));
   if(type != VT_BSTR)
   {
      std::string error = "Unexpected data type reading of compound stream. Expected string. Received:";
      error += std::to_string(type);
      throw std::runtime_error(error);
   }
   int32_t string_length;
   stream.read((char*)&string_length, sizeof(string_length));
   std::string value;
   if(string_length > 0)
   {
      std::vector<char> buffer(string_length, 0);
      stream.read((char*)buffer.data(), string_length);
      std::u16string src((char16_t*)buffer.data());
      std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
      value = convert.to_bytes(src);
   }
   return value;
}
