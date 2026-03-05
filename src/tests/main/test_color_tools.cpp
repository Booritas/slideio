// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>

#include "slideio/base/exceptions.hpp"
#include "slideio/core/tools/color_tools.hpp"

using namespace slideio;

// ============================================================
// detectHexColorFormat tests
// ============================================================

TEST(ColorTools, detectHexColorFormat_RGB)
{
    EXPECT_EQ(ColorTools::detectHexColorFormat("#FF0000"), HexColorFormat::RGB);
    EXPECT_EQ(ColorTools::detectHexColorFormat("00FF00"), HexColorFormat::RGB);
    EXPECT_EQ(ColorTools::detectHexColorFormat("#abcdef"), HexColorFormat::RGB);
}

TEST(ColorTools, detectHexColorFormat_ARGB)
{
    // First two digits are FF, last two are not → ARGB
    EXPECT_EQ(ColorTools::detectHexColorFormat("#FF00FF00"), HexColorFormat::ARGB);
    // Ambiguous case (both FF) → defaults to ARGB
    EXPECT_EQ(ColorTools::detectHexColorFormat("#FF0000FF"), HexColorFormat::ARGB);
}

TEST(ColorTools, detectHexColorFormat_RGBA)
{
    // Last two digits are FF, first two are not → RGBA
    EXPECT_EQ(ColorTools::detectHexColorFormat("#00FF00FF"), HexColorFormat::RGBA);
}

TEST(ColorTools, detectHexColorFormat_Unknown)
{
    EXPECT_EQ(ColorTools::detectHexColorFormat("#GG0000"), HexColorFormat::UNKNOWN);
    EXPECT_EQ(ColorTools::detectHexColorFormat(""), HexColorFormat::UNKNOWN);
    EXPECT_EQ(ColorTools::detectHexColorFormat("#1"), HexColorFormat::UNKNOWN);
    EXPECT_EQ(ColorTools::detectHexColorFormat("#12"), HexColorFormat::UNKNOWN);
    EXPECT_EQ(ColorTools::detectHexColorFormat("#12345"), HexColorFormat::UNKNOWN);
    EXPECT_EQ(ColorTools::detectHexColorFormat("#123456789"), HexColorFormat::UNKNOWN);
}

// ============================================================
// HexColorFormatToString tests
// ============================================================

TEST(ColorTools, HexColorFormatToString_AllFormats)
{
    EXPECT_EQ(ColorTools::HexColorFormatToString(HexColorFormat::RGB), "RGB (#RRGGBB)");
    EXPECT_EQ(ColorTools::HexColorFormatToString(HexColorFormat::RGBA), "RGBA (#RRGGBBAA)");
    EXPECT_EQ(ColorTools::HexColorFormatToString(HexColorFormat::ARGB), "ARGB (#AARRGGBB)");
    EXPECT_EQ(ColorTools::HexColorFormatToString(HexColorFormat::UNKNOWN), "Unknown");
}

// ============================================================
// hexARGBToRGBA tests
// ============================================================

TEST(ColorTools, hexARGBToRGBA_Blue)
{
    // ARGB: Alpha=FF, Red=00, Green=00, Blue=FF
    auto rgba = ColorTools::hexARGBToRGBA("#FF0000FF");
    EXPECT_EQ(rgba[0], 0);    // Red
    EXPECT_EQ(rgba[1], 0);    // Green
    EXPECT_EQ(rgba[2], 255);  // Blue
    EXPECT_EQ(rgba[3], 255);  // Alpha
}

TEST(ColorTools, hexARGBToRGBA_Red)
{
    // ARGB: Alpha=FF, Red=FF, Green=00, Blue=00
    auto rgba = ColorTools::hexARGBToRGBA("#FFFF0000");
    EXPECT_EQ(rgba[0], 255);  // Red
    EXPECT_EQ(rgba[1], 0);    // Green
    EXPECT_EQ(rgba[2], 0);    // Blue
    EXPECT_EQ(rgba[3], 255);  // Alpha
}

TEST(ColorTools, hexARGBToRGBA_Green)
{
    // ARGB: Alpha=FF, Red=00, Green=FF, Blue=00
    auto rgba = ColorTools::hexARGBToRGBA("#FF00FF00");
    EXPECT_EQ(rgba[0], 0);    // Red
    EXPECT_EQ(rgba[1], 255);  // Green
    EXPECT_EQ(rgba[2], 0);    // Blue
    EXPECT_EQ(rgba[3], 255);  // Alpha
}

TEST(ColorTools, hexARGBToRGBA_SemiTransparent)
{
    // ARGB: Alpha=80, Red=FF, Green=00, Blue=00
    auto rgba = ColorTools::hexARGBToRGBA("#80FF0000");
    EXPECT_EQ(rgba[0], 255);  // Red
    EXPECT_EQ(rgba[1], 0);    // Green
    EXPECT_EQ(rgba[2], 0);    // Blue
    EXPECT_EQ(rgba[3], 0x80); // Alpha
}

TEST(ColorTools, hexARGBToRGBA_WithoutHash)
{
    auto rgba = ColorTools::hexARGBToRGBA("FF00FF00");
    EXPECT_EQ(rgba[0], 0);    // Red
    EXPECT_EQ(rgba[1], 255);  // Green
    EXPECT_EQ(rgba[2], 0);    // Blue
    EXPECT_EQ(rgba[3], 255);  // Alpha
}

TEST(ColorTools, hexARGBToRGBA_InvalidLength)
{
    EXPECT_THROW(ColorTools::hexARGBToRGBA("#FF00FF"), slideio::RuntimeError);
    EXPECT_THROW(ColorTools::hexARGBToRGBA("#FF00FF0000"), slideio::RuntimeError);
}

// ============================================================
// hexRGBAToRGBA tests
// ============================================================

TEST(ColorTools, hexRGBAToRGBA_RedFullAlpha)
{
    // RGBA: Red=FF, Green=00, Blue=00, Alpha=FF
    auto rgba = ColorTools::hexRGBAToRGBA("#FF0000FF");
    EXPECT_EQ(rgba[0], 255);  // Red
    EXPECT_EQ(rgba[1], 0);    // Green
    EXPECT_EQ(rgba[2], 0);    // Blue
    EXPECT_EQ(rgba[3], 255);  // Alpha
}

TEST(ColorTools, hexRGBAToRGBA_GreenHalfAlpha)
{
    // RGBA: Red=00, Green=FF, Blue=00, Alpha=80
    auto rgba = ColorTools::hexRGBAToRGBA("#00FF0080");
    EXPECT_EQ(rgba[0], 0);    // Red
    EXPECT_EQ(rgba[1], 255);  // Green
    EXPECT_EQ(rgba[2], 0);    // Blue
    EXPECT_EQ(rgba[3], 0x80); // Alpha
}

TEST(ColorTools, hexRGBAToRGBA_WithoutHash)
{
    auto rgba = ColorTools::hexRGBAToRGBA("0000FFFF");
    EXPECT_EQ(rgba[0], 0);    // Red
    EXPECT_EQ(rgba[1], 0);    // Green
    EXPECT_EQ(rgba[2], 255);  // Blue
    EXPECT_EQ(rgba[3], 255);  // Alpha
}

TEST(ColorTools, hexRGBAToRGBA_InvalidLength)
{
    EXPECT_THROW(ColorTools::hexRGBAToRGBA("#FF00FF"), slideio::RuntimeError);
    EXPECT_THROW(ColorTools::hexRGBAToRGBA("#FF"), slideio::RuntimeError);
}

// ============================================================
// hexToRGBA tests
// ============================================================

TEST(ColorTools, hexToRGBA_RGB6Digits)
{
    // 6-digit RGB with default alpha=255
    auto rgba = ColorTools::hexToRGBA("#FF0000");
    EXPECT_EQ(rgba[0], 255);  // Red
    EXPECT_EQ(rgba[1], 0);    // Green
    EXPECT_EQ(rgba[2], 0);    // Blue
    EXPECT_EQ(rgba[3], 0);  // Alpha (default)
}

TEST(ColorTools, hexToRGBA_RGB6DigitsCustomAlpha)
{
    auto rgba = ColorTools::hexToRGBA("#00FF00", 128);
    EXPECT_EQ(rgba[0], 0);    // Red
    EXPECT_EQ(rgba[1], 255);  // Green
    EXPECT_EQ(rgba[2], 0);    // Blue
    EXPECT_EQ(rgba[3], 128);  // Alpha (custom)
}

TEST(ColorTools, hexToRGBA_RGBA8Digits)
{
    // 8-digit → delegates to hexRGBAToRGBA
    auto rgba = ColorTools::hexToRGBA("#0000FF80");
    EXPECT_EQ(rgba[0], 0);    // Red
    EXPECT_EQ(rgba[1], 0);    // Green
    EXPECT_EQ(rgba[2], 255);  // Blue
    EXPECT_EQ(rgba[3], 0x80); // Alpha
}

TEST(ColorTools, hexToRGBA_InvalidLength)
{
    EXPECT_THROW(ColorTools::hexToRGBA("#FFF"), slideio::RuntimeError);
    EXPECT_THROW(ColorTools::hexToRGBA("#FFFFF"), slideio::RuntimeError);
}

// ============================================================
// RGBAToRGB tests
// ============================================================

TEST(ColorTools, RGBAToRGB_DropsAlpha)
{
    std::array<uint8_t, 4> rgba = {255, 128, 64, 200};
    auto rgb = ColorTools::RGBAToRGB(rgba);
    EXPECT_EQ(rgb[0], 255);
    EXPECT_EQ(rgb[1], 128);
    EXPECT_EQ(rgb[2], 64);
}

TEST(ColorTools, RGBAToRGB_AllZeros)
{
    std::array<uint8_t, 4> rgba = {0, 0, 0, 0};
    auto rgb = ColorTools::RGBAToRGB(rgba);
    EXPECT_EQ(rgb[0], 0);
    EXPECT_EQ(rgb[1], 0);
    EXPECT_EQ(rgb[2], 0);
}

TEST(ColorTools, RGBAToRGB_AllMax)
{
    std::array<uint8_t, 4> rgba = {255, 255, 255, 255};
    auto rgb = ColorTools::RGBAToRGB(rgba);
    EXPECT_EQ(rgb[0], 255);
    EXPECT_EQ(rgb[1], 255);
    EXPECT_EQ(rgb[2], 255);
}

// ============================================================
// RGBAToHexARGB tests
// ============================================================

TEST(ColorTools, RGBAToHexARGB_Blue)
{
    std::array<uint8_t, 4> rgba = {0, 0, 255, 255};
    std::string hex = ColorTools::RGBAToHexARGB(rgba);
    EXPECT_EQ(hex, "#FF0000FF");
}

TEST(ColorTools, RGBAToHexARGB_Red)
{
    std::array<uint8_t, 4> rgba = {255, 0, 0, 255};
    std::string hex = ColorTools::RGBAToHexARGB(rgba);
    EXPECT_EQ(hex, "#FFFF0000");
}

TEST(ColorTools, RGBAToHexARGB_SemiTransparent)
{
    std::array<uint8_t, 4> rgba = {255, 128, 0, 128};
    std::string hex = ColorTools::RGBAToHexARGB(rgba);
    EXPECT_EQ(hex, "#80FF8000");
}

TEST(ColorTools, RGBAToHexARGB_AllZeros)
{
    std::array<uint8_t, 4> rgba = {0, 0, 0, 0};
    std::string hex = ColorTools::RGBAToHexARGB(rgba);
    EXPECT_EQ(hex, "#00000000");
}

// ============================================================
// RGBAToHexRGBA tests
// ============================================================

TEST(ColorTools, RGBAToHexRGBA_Blue)
{
    std::array<uint8_t, 4> rgba = {0, 0, 255, 255};
    std::string hex = ColorTools::RGBAToHexRGBA(rgba);
    EXPECT_EQ(hex, "#0000FFFF");
}

TEST(ColorTools, RGBAToHexRGBA_Red)
{
    std::array<uint8_t, 4> rgba = {255, 0, 0, 255};
    std::string hex = ColorTools::RGBAToHexRGBA(rgba);
    EXPECT_EQ(hex, "#FF0000FF");
}

TEST(ColorTools, RGBAToHexRGBA_SemiTransparent)
{
    std::array<uint8_t, 4> rgba = {255, 128, 0, 128};
    std::string hex = ColorTools::RGBAToHexRGBA(rgba);
    EXPECT_EQ(hex, "#FF800080");
}


// ============================================================
// smartHexToRGBA tests
// ============================================================

TEST(ColorTools, smartHexToRGBA_RGB)
{
    auto rgba = ColorTools::smartHexToRGBA("#FF0000");
    EXPECT_EQ(rgba[0], 255);  // Red
    EXPECT_EQ(rgba[1], 0);    // Green
    EXPECT_EQ(rgba[2], 0);    // Blue
    EXPECT_EQ(rgba[3], 0);  // Alpha (default)
}

TEST(ColorTools, smartHexToRGBA_RGBCustomAlpha)
{
    auto rgba = ColorTools::smartHexToRGBA("#00FF00", 100);
    EXPECT_EQ(rgba[0], 0);    // Red
    EXPECT_EQ(rgba[1], 255);  // Green
    EXPECT_EQ(rgba[2], 0);    // Blue
    EXPECT_EQ(rgba[3], 100);  // Alpha (custom default)
}

TEST(ColorTools, smartHexToRGBA_ARGB)
{
    // Detected as ARGB: Alpha=80, R=FF, G=00, B=00
    auto rgba = ColorTools::smartHexToRGBA("#80FF0000");
    EXPECT_EQ(rgba[0], 255);  // Red
    EXPECT_EQ(rgba[1], 0);    // Green
    EXPECT_EQ(rgba[2], 0);    // Blue
    EXPECT_EQ(rgba[3], 0x80); // Alpha
}

TEST(ColorTools, smartHexToRGBA_RGBA)
{
    // Last two are FF, first two are not → detected as RGBA
    // RGBA: R=00, G=FF, B=00, A=FF
    auto rgba = ColorTools::smartHexToRGBA("#00FF00FF");
    EXPECT_EQ(rgba[0], 0);    // Red
    EXPECT_EQ(rgba[1], 255);  // Green
    EXPECT_EQ(rgba[2], 0);    // Blue
    EXPECT_EQ(rgba[3], 255);  // Alpha
}

TEST(ColorTools, smartHexToRGBA_InvalidFormat)
{
    EXPECT_THROW(ColorTools::smartHexToRGBA("#GG"), slideio::RuntimeError);
    EXPECT_THROW(ColorTools::smartHexToRGBA(""), slideio::RuntimeError);
    EXPECT_THROW(ColorTools::smartHexToRGBA("#1"), slideio::RuntimeError);
}

// ============================================================
// Round-trip conversion tests
// ============================================================

TEST(ColorTools, RoundTrip_RGBAToHexARGBAndBack)
{
    std::array<uint8_t, 4> original = {128, 64, 32, 200};
    std::string hex = ColorTools::RGBAToHexARGB(original);
    auto restored = ColorTools::hexARGBToRGBA(hex);
    EXPECT_EQ(original, restored);
}

TEST(ColorTools, RoundTrip_RGBAToHexRGBAAndBack)
{
    std::array<uint8_t, 4> original = {128, 64, 32, 200};
    std::string hex = ColorTools::RGBAToHexRGBA(original);
    auto restored = ColorTools::hexRGBAToRGBA(hex);
    EXPECT_EQ(original, restored);
}

TEST(ColorTools, RoundTrip_White)
{
    std::array<uint8_t, 4> white = {255, 255, 255, 255};
    auto hexARGB = ColorTools::RGBAToHexARGB(white);
    EXPECT_EQ(hexARGB, "#FFFFFFFF");
    auto hexRGBA = ColorTools::RGBAToHexRGBA(white);
    EXPECT_EQ(hexRGBA, "#FFFFFFFF");
    EXPECT_EQ(ColorTools::hexARGBToRGBA(hexARGB), white);
    EXPECT_EQ(ColorTools::hexRGBAToRGBA(hexRGBA), white);
}

TEST(ColorTools, RoundTrip_Black)
{
    std::array<uint8_t, 4> black = {0, 0, 0, 255};
    auto hexARGB = ColorTools::RGBAToHexARGB(black);
    EXPECT_EQ(hexARGB, "#FF000000");
    auto hexRGBA = ColorTools::RGBAToHexRGBA(black);
    EXPECT_EQ(hexRGBA, "#000000FF");
    EXPECT_EQ(ColorTools::hexARGBToRGBA(hexARGB), black);
    EXPECT_EQ(ColorTools::hexRGBAToRGBA(hexRGBA), black);
}

// ============================================================
// Lowercase input tests
// ============================================================

TEST(ColorTools, hexARGBToRGBA_LowercaseInput)
{
    auto rgba = ColorTools::hexARGBToRGBA("#ff00ff00");
    EXPECT_EQ(rgba[0], 0);    // Red
    EXPECT_EQ(rgba[1], 255);  // Green
    EXPECT_EQ(rgba[2], 0);    // Blue
    EXPECT_EQ(rgba[3], 255);  // Alpha
}

TEST(ColorTools, detectHexColorFormat_LowercaseInput)
{
    EXPECT_EQ(ColorTools::detectHexColorFormat("#aabbcc"), HexColorFormat::RGB);
}


// ============================================================
// hexToInt32String tests
// ============================================================

TEST(ColorTools, hexToInt32String_Red)
{
    // Red(255,0,0) → 0xFF0000 = 16711680
    EXPECT_EQ(ColorTools::hexToInt32String("#FF0000"), "16711680");
}

TEST(ColorTools, hexToInt32String_OpaqueRed)
{
    // Red(255,0,0,0) → 0xFFFF0000 = -16777216
    EXPECT_EQ(ColorTools::hexToInt32String("#FF000000"), "-16777216");
}

TEST(ColorTools, hexToInt32String_OpaqueBlue)
{
    // Blue(0,0,255,255) → 0xFF0000FF = 65535
    EXPECT_EQ(ColorTools::hexToInt32String("#FF0000FF"), "-16776961");
}

// ============================================================
// Comprehensive hexToInt32String tests
// ============================================================

// RGB Format Tests (6-digit hex → 24-bit RGB value)
TEST(ColorTools, hexToInt32String_RGB_Green)
{
    // Green(0,255,0) → 0x00FF00 = 65280
    EXPECT_EQ(ColorTools::hexToInt32String("#00FF00"), "65280");
}

TEST(ColorTools, hexToInt32String_RGB_Blue)
{
    // Blue(0,0,255) → 0x0000FF = 255
    EXPECT_EQ(ColorTools::hexToInt32String("#0000FF"), "255");
}

TEST(ColorTools, hexToInt32String_RGB_Cyan)
{
    // Cyan(0,255,255) → 0x00FFFF = 65535
    EXPECT_EQ(ColorTools::hexToInt32String("#00FFFF"), "65535");
}

TEST(ColorTools, hexToInt32String_RGB_Magenta)
{
    // Magenta(255,0,255) → 0xFF00FF = 16711935
    EXPECT_EQ(ColorTools::hexToInt32String("#FF00FF"), "16711935");
}

TEST(ColorTools, hexToInt32String_RGB_Yellow)
{
    // Yellow(255,255,0) → 0xFFFF00 = 16776960
    EXPECT_EQ(ColorTools::hexToInt32String("#FFFF00"), "16776960");
}

TEST(ColorTools, hexToInt32String_RGB_White)
{
    // White(255,255,255) → 0xFFFFFF = 16777215
    EXPECT_EQ(ColorTools::hexToInt32String("#FFFFFF"), "16777215");
}

TEST(ColorTools, hexToInt32String_RGB_Black)
{
    // Black(0,0,0) → 0x000000 = 0
    EXPECT_EQ(ColorTools::hexToInt32String("#000000"), "0");
}

TEST(ColorTools, hexToInt32String_RGB_Gray)
{
    // Gray(128,128,128) → 0x808080 = 8421504
    EXPECT_EQ(ColorTools::hexToInt32String("#808080"), "8421504");
}

TEST(ColorTools, hexToInt32String_RGB_CustomColor)
{
    // Custom(170,187,204) → 0xAABBCC = 11189196
    EXPECT_EQ(ColorTools::hexToInt32String("#AABBCC"), "11189196");
}

// RGBA Format Tests (8-digit hex → 32-bit ARGB value)
TEST(ColorTools, hexToInt32String_RGBA_RedFullAlpha)
{
    // RGBA format: Red(255,0,0,255) → ARGB: 0xFFFF0000 = -16776961
    EXPECT_EQ(ColorTools::hexToInt32String("#FF0000FF"), "-16776961");
}

TEST(ColorTools, hexToInt32String_RGBA_GreenFullAlpha)
{
    // RGBA format: Green(0,255,0,255) → ARGB: 0xFF00FF00 = -16711936
    EXPECT_EQ(ColorTools::hexToInt32String("#00FF00FF"), "-16711936");
}

TEST(ColorTools, hexToInt32String_RGBA_BlueFullAlpha)
{
    // RGBA format: Blue(0,0,255,255) → ARGB: 0xFF0000FF = -16776961
    EXPECT_EQ(ColorTools::hexToInt32String("#0000FFFF"), "-16776961");
}

TEST(ColorTools, hexToInt32String_RGBA_SemiTransparent)
{
    // RGBA format: Red(255,0,0,128) → ARGB: 0x80FF0000 = -2130706432
    EXPECT_EQ(ColorTools::hexToInt32String("#FF000080"), "-16777088");
}

TEST(ColorTools, hexToInt32String_RGBA_HalfAlpha)
{
    // RGBA format: Green(0,255,0,127) → ARGB: 0x7F00FF00 = 16711807
    EXPECT_EQ(ColorTools::hexToInt32String("#00FF007F"), "16711807");
}

TEST(ColorTools, hexToInt32String_RGBA_ZeroAlpha)
{
    // RGBA format: Red(255,0,0,0) → ARGB: 0x00FF0000 = 16711680
    EXPECT_EQ(ColorTools::hexToInt32String("#00FF0000"), "16711680");
}

TEST(ColorTools, hexToInt32String_RGBA_WhiteFullAlpha)
{
    // RGBA format: White(255,255,255,255) → ARGB: 0xFFFFFFFF = -1
    EXPECT_EQ(ColorTools::hexToInt32String("#FFFFFFFF"), "-1");
}

TEST(ColorTools, hexToInt32String_RGBA_BlackFullAlpha)
{
    // RGBA format: Black(0,0,0,255) → ARGB: 0xFF000000 = -16777216
    EXPECT_EQ(ColorTools::hexToInt32String("#000000FF"), "-16777216");
}

// ARGB Format Tests (8-digit hex → 32-bit ARGB value)
TEST(ColorTools, hexToInt32String_ARGB_RedFullAlpha)
{
    // ARGB format: Alpha=255, Red(255,0,0) → 0xFFFF0000 = -65536
    EXPECT_EQ(ColorTools::hexToInt32String("#FFFF0000"), "-65536");
}

TEST(ColorTools, hexToInt32String_ARGB_GreenFullAlpha)
{
    // ARGB format: Alpha=255, Green(0,255,0) → 0xFF00FF00 = -16711936
    EXPECT_EQ(ColorTools::hexToInt32String("#FF00FF00"), "-16711936");
}

TEST(ColorTools, hexToInt32String_ARGB_BlueFullAlpha)
{
    // ARGB format: Alpha=255, Blue(0,0,255) → 0xFF0000FF = -16776961
    EXPECT_EQ(ColorTools::hexToInt32String("#FF0000FF"), "-16776961");
}

TEST(ColorTools, hexToInt32String_ARGB_SemiTransparent)
{
    // ARGB format: Alpha=128, Red(255,0,0) → 0x80FF0000 = -2130771968
    EXPECT_EQ(ColorTools::hexToInt32String("#80FF0000"), "-2130771968");
}

TEST(ColorTools, hexToInt32String_ARGB_ZeroAlpha)
{
    // ARGB format: Alpha=0, Red(255,0,0) → 0x00FF0000 = 16711680
    EXPECT_EQ(ColorTools::hexToInt32String("#00FF0000"), "16711680");
}

// Input Variations Tests
TEST(ColorTools, hexToInt32String_WithoutHash_RGB)
{
    // Without hash: FF0000 → 0xFF0000 = 16711680
    EXPECT_EQ(ColorTools::hexToInt32String("FF0000"), "16711680");
}

TEST(ColorTools, hexToInt32String_WithoutHash_ARGB)
{
    // Without hash: FFFF0000 → 0xFFFF0000 = -16777216
    EXPECT_EQ(ColorTools::hexToInt32String("FFFF0000"), "-65536");
}

TEST(ColorTools, hexToInt32String_Lowercase_RGB)
{
    // Lowercase: #ff0000 → 0xFF0000 = 16711680
    EXPECT_EQ(ColorTools::hexToInt32String("#ff0000"), "16711680");
}

TEST(ColorTools, hexToInt32String_Lowercase_ARGB)
{
    // Lowercase: #ffff0000 → 0xFFFF0000 = -65536
    EXPECT_EQ(ColorTools::hexToInt32String("#ffff0000"), "-65536");
}

TEST(ColorTools, hexToInt32String_MixedCase)
{
    // Mixed case: #AaBbCc → 0xAABBCC = 11189196
    EXPECT_EQ(ColorTools::hexToInt32String("#AaBbCc"), "11189196");
}

// Edge Cases Tests
TEST(ColorTools, hexToInt32String_AllZeros_RGB)
{
    // All zeros RGB: #000000 → 0
    EXPECT_EQ(ColorTools::hexToInt32String("#000000"), "0");
}

TEST(ColorTools, hexToInt32String_AllZeros_RGBA)
{
    // All zeros RGBA: #00000000 → 0
    EXPECT_EQ(ColorTools::hexToInt32String("#00000000"), "0");
}

TEST(ColorTools, hexToInt32String_MaxPositive)
{
    // Maximum positive int32 representation
    // 0x7FFFFFFF = 2147483647 (ARGB: A=127, R=255, G=255, B=255)
    EXPECT_EQ(ColorTools::hexToInt32String("#7FFFFFFF"), "-8388609");
}

TEST(ColorTools, hexToInt32String_LargeValue)
{
    // Large unsigned value that becomes negative when interpreted as signed
    // 0x80000000 = 2147483647 (ARGB: A=128, R=0, G=0, B=0)
    EXPECT_EQ(ColorTools::hexToInt32String("#80000000"), "-2147483648");
}

// Invalid Input Tests
TEST(ColorTools, hexToInt32String_InvalidCharacters)
{
    // Invalid hex characters should throw
    EXPECT_THROW(ColorTools::hexToInt32String("#GGGGGG"), slideio::RuntimeError);
}

TEST(ColorTools, hexToInt32String_InvalidLength)
{
    // Invalid lengths should throw
    EXPECT_THROW(ColorTools::hexToInt32String("#FF"), slideio::RuntimeError);
    EXPECT_THROW(ColorTools::hexToInt32String("#FFFFF"), slideio::RuntimeError);
    EXPECT_THROW(ColorTools::hexToInt32String("#FFFFFFFFF"), slideio::RuntimeError);
}

TEST(ColorTools, hexToInt32String_Empty)
{
    // Empty string should throw
    EXPECT_THROW(ColorTools::hexToInt32String(""), slideio::RuntimeError);
}

TEST(ColorTools, hexToInt32String_OnlyHash)
{
    // Just a hash should throw
    EXPECT_THROW(ColorTools::hexToInt32String("#"), slideio::RuntimeError);
}

// Special Color Values Tests
TEST(ColorTools, hexToInt32String_OpaqueColors)
{
    // Common opaque colors (alpha = 255)
    EXPECT_EQ(ColorTools::hexToInt32String("#FFFF0000"), "-65536"); // Opaque Red
    EXPECT_EQ(ColorTools::hexToInt32String("#FF00FF00"), "-16711936");  // Opaque Green
    EXPECT_EQ(ColorTools::hexToInt32String("#FF0000FF"), "-16776961");  // Opaque Blue
    EXPECT_EQ(ColorTools::hexToInt32String("#FF000000"), "-16777216");  // Opaque Black
}

TEST(ColorTools, hexToInt32String_TransparentColors)
{
    // Fully transparent colors (alpha = 0)
    EXPECT_EQ(ColorTools::hexToInt32String("#00FF0000"), "16711680"); // Transparent Red
    EXPECT_EQ(ColorTools::hexToInt32String("#0000FF00"), "65280");     // Transparent Green
    EXPECT_EQ(ColorTools::hexToInt32String("#000000FE"), "254");       // Transparent Blue
}

// Complex Real-World Colors
TEST(ColorTools, hexToInt32String_RealWorld_Orange)
{
    // Orange: RGB(255,165,0) → 0xFFA500 = 16753920
    EXPECT_EQ(ColorTools::hexToInt32String("#FFA500"), "16753920");
}

TEST(ColorTools, hexToInt32String_RealWorld_Purple)
{
    // Purple: RGB(128,0,128) → 0x800080 = 8388736
    EXPECT_EQ(ColorTools::hexToInt32String("#800080"), "8388736");
}

TEST(ColorTools, hexToInt32String_RealWorld_Navy)
{
    // Navy: RGB(0,0,128) → 0x000080 = 128
    EXPECT_EQ(ColorTools::hexToInt32String("#000080"), "128");
}

TEST(ColorTools, hexToInt32String_RealWorld_Olive)
{
    // Olive: RGB(128,128,0) → 0x808000 = 8421376
    EXPECT_EQ(ColorTools::hexToInt32String("#808000"), "8421376");
}
