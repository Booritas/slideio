// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.#pragma once
#include "slideio/slideio_def.hpp"
#include <codecvt>
#include <string>
#include <pole/polepp.hpp>

namespace ZVIUtils
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

   void SLIDEIO_EXPORTS skipItem(ole::basic_stream& stream);
   void SLIDEIO_EXPORTS skipItems(ole::basic_stream& stream, int count);
   int32_t SLIDEIO_EXPORTS readIntItem(ole::basic_stream& stream);
   double SLIDEIO_EXPORTS readDoubleItem(ole::basic_stream& stream);
   std::string SLIDEIO_EXPORTS readStringItem(ole::basic_stream& stream);
}
