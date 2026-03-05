// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/core/tools/color_tools.hpp"

using namespace slideio;

// ============================================================
// detectColorFormat tests
// ============================================================

TEST(ColorTools, detectColorFormat_RGB)
{
    EXPECT_EQ(ColorTools::detectColorFormat("#FF0000"), ColorFormat::RGB);
    EXPECT_EQ(ColorTools::detectColorFormat("00FF00"), ColorFormat::RGB);
    EXPECT_EQ(ColorTools::detectColorFormat("#abcdef"), ColorFormat::RGB);
}

TEST(ColorTools, detectColorFormat_ShortRGB)
{
    EXPECT_EQ(ColorTools::detectColorFormat("#F00"), ColorFormat::SHORT_RGB);
    EXPECT_EQ(ColorTools::detectColorFormat("abc"), ColorFormat::SHORT_RGB);
}

TEST(ColorTools, detectColorFormat_ShortRGBA)
{
    EXPECT_EQ(ColorTools::detectColorFormat("#F00A"), ColorFormat::SHORT_RGBA);
    EXPECT_EQ(ColorTools::detectColorFormat("abcd"), ColorFormat::SHORT_RGBA);
}

TEST(ColorTools, detectColorFormat_ARGB)
{
    // First two digits are FF, last two are not → ARGB
    EXPECT_EQ(ColorTools::detectColorFormat("#FF00FF00"), ColorFormat::ARGB);
    // Ambiguous case (both FF) → defaults to ARGB
    EXPECT_EQ(ColorTools::detectColorFormat("#FF0000FF"), ColorFormat::ARGB);
}

TEST(ColorTools, detectColorFormat_RGBA)
{
    // Last two digits are FF, first two are not → RGBA
    EXPECT_EQ(ColorTools::detectColorFormat("#00FF00FF"), ColorFormat::RGBA);
}

TEST(ColorTools, detectColorFormat_Unknown)
{
    EXPECT_EQ(ColorTools::detectColorFormat("#GG0000"), ColorFormat::UNKNOWN);
    EXPECT_EQ(ColorTools::detectColorFormat(""), ColorFormat::UNKNOWN);
    EXPECT_EQ(ColorTools::detectColorFormat("#1"), ColorFormat::UNKNOWN);
    EXPECT_EQ(ColorTools::detectColorFormat("#12"), ColorFormat::UNKNOWN);
    EXPECT_EQ(ColorTools::detectColorFormat("#12345"), ColorFormat::UNKNOWN);
    EXPECT_EQ(ColorTools::detectColorFormat("#123456789"), ColorFormat::UNKNOWN);
}

// ============================================================
// colorFormatToString tests
// ============================================================

TEST(ColorTools, colorFormatToString_AllFormats)
{
    EXPECT_EQ(ColorTools::colorFormatToString(ColorFormat::RGB), "RGB (#RRGGBB)");
    EXPECT_EQ(ColorTools::colorFormatToString(ColorFormat::RGBA), "RGBA (#RRGGBBAA)");
    EXPECT_EQ(ColorTools::colorFormatToString(ColorFormat::ARGB), "ARGB (#AARRGGBB)");
    EXPECT_EQ(ColorTools::colorFormatToString(ColorFormat::SHORT_RGB), "Short RGB (#RGB)");
    EXPECT_EQ(ColorTools::colorFormatToString(ColorFormat::SHORT_RGBA), "Short RGBA (#RGBA)");
    EXPECT_EQ(ColorTools::colorFormatToString(ColorFormat::UNKNOWN), "Unknown");
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
    EXPECT_THROW(ColorTools::hexARGBToRGBA("#FF00FF"), std::invalid_argument);
    EXPECT_THROW(ColorTools::hexARGBToRGBA("#FF00FF0000"), std::invalid_argument);
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
    EXPECT_THROW(ColorTools::hexRGBAToRGBA("#FF00FF"), std::invalid_argument);
    EXPECT_THROW(ColorTools::hexRGBAToRGBA("#FF"), std::invalid_argument);
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
    EXPECT_EQ(rgba[3], 255);  // Alpha (default)
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
    EXPECT_THROW(ColorTools::hexToRGBA("#FFF"), std::invalid_argument);
    EXPECT_THROW(ColorTools::hexToRGBA("#FFFFF"), std::invalid_argument);
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
// expandShortHex tests
// ============================================================

TEST(ColorTools, expandShortHex_RGB)
{
    EXPECT_EQ(ColorTools::expandShortHex("#ABC"), "#AABBCC");
    EXPECT_EQ(ColorTools::expandShortHex("#F00"), "#FF0000");
}

TEST(ColorTools, expandShortHex_RGBA)
{
    EXPECT_EQ(ColorTools::expandShortHex("#ABCD"), "#AABBCCDD");
    EXPECT_EQ(ColorTools::expandShortHex("#F00F"), "#FF0000FF");
}

TEST(ColorTools, expandShortHex_WithoutHash)
{
    EXPECT_EQ(ColorTools::expandShortHex("ABC"), "#AABBCC");
    EXPECT_EQ(ColorTools::expandShortHex("ABCD"), "#AABBCCDD");
}

TEST(ColorTools, expandShortHex_AlreadyFull)
{
    // Non-short formats should be returned as-is
    EXPECT_EQ(ColorTools::expandShortHex("#AABBCC"), "#AABBCC");
    EXPECT_EQ(ColorTools::expandShortHex("#AABBCCDD"), "#AABBCCDD");
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
    EXPECT_EQ(rgba[3], 255);  // Alpha (default)
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

TEST(ColorTools, smartHexToRGBA_ShortRGB)
{
    // #F00 → expands to #FF0000 → parsed as RGB
    auto rgba = ColorTools::smartHexToRGBA("#F00");
    EXPECT_EQ(rgba[0], 255);  // Red
    EXPECT_EQ(rgba[1], 0);    // Green
    EXPECT_EQ(rgba[2], 0);    // Blue
    EXPECT_EQ(rgba[3], 255);  // Alpha (default)
}

TEST(ColorTools, smartHexToRGBA_ShortRGBA)
{
    // #F008 → expands to #FF000088 → detected format and parsed
    auto rgba = ColorTools::smartHexToRGBA("#F008");
    // Expanded: #FF000088
    // First two = FF, last two = 88 → detected as ARGB
    // ARGB: A=FF, R=00, G=00, B=88
    EXPECT_EQ(rgba[0], 0);    // Red
    EXPECT_EQ(rgba[1], 0);    // Green
    EXPECT_EQ(rgba[2], 0x88); // Blue
    EXPECT_EQ(rgba[3], 255);  // Alpha
}

TEST(ColorTools, smartHexToRGBA_InvalidFormat)
{
    EXPECT_THROW(ColorTools::smartHexToRGBA("#GG"), std::invalid_argument);
    EXPECT_THROW(ColorTools::smartHexToRGBA(""), std::invalid_argument);
    EXPECT_THROW(ColorTools::smartHexToRGBA("#1"), std::invalid_argument);
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

TEST(ColorTools, detectColorFormat_LowercaseInput)
{
    EXPECT_EQ(ColorTools::detectColorFormat("#aabbcc"), ColorFormat::RGB);
    EXPECT_EQ(ColorTools::detectColorFormat("#abc"), ColorFormat::SHORT_RGB);
}

// ============================================================
// smartHexToRGBAString tests
// ============================================================

TEST(ColorTools, smartHexToRGBAString_OpaqueRed)
{
    EXPECT_EQ(ColorTools::smartHexToRGBAString("#FF0000"), "rgba(255, 0, 0, 1.00)");
}

TEST(ColorTools, smartHexToRGBAString_OpaqueBlue)
{
    EXPECT_EQ(ColorTools::smartHexToRGBAString("#0000FF"), "rgba(0, 0, 255, 1.00)");
}

TEST(ColorTools, smartHexToRGBAString_SemiTransparentARGB)
{
    // Detected as ARGB: Alpha=80, R=FF, G=00, B=00
    EXPECT_EQ(ColorTools::smartHexToRGBAString("#80FF0000"), "rgba(255, 0, 0, 0.50)");
}

TEST(ColorTools, smartHexToRGBAString_CustomDefaultAlpha)
{
    // RGB with custom default alpha of 0 → fully transparent
    EXPECT_EQ(ColorTools::smartHexToRGBAString("#00FF00", 0), "rgba(0, 255, 0, 0.00)");
}

TEST(ColorTools, smartHexToRGBAString_ShortRGB)
{
    // #F00 → red, fully opaque
    EXPECT_EQ(ColorTools::smartHexToRGBAString("#F00"), "rgba(255, 0, 0, 1.00)");
}

TEST(ColorTools, smartHexToRGBAString_InvalidThrows)
{
    EXPECT_THROW(ColorTools::smartHexToRGBAString(""), std::invalid_argument);
    EXPECT_THROW(ColorTools::smartHexToRGBAString("#GG"), std::invalid_argument);
}

// ============================================================
// hexToRGBAInt32String tests
// ============================================================

TEST(ColorTools, hexToRGBAInt32String_OpaqueRed)
{
    // Red(255,0,0,255) → 0xFF0000FF = 4278190335
    EXPECT_EQ(ColorTools::hexToRGBAInt32String("#FF0000"), "4278190335");
}

TEST(ColorTools, hexToRGBAInt32String_OpaqueGreen)
{
    // Green(0,255,0,255) → 0x00FF00FF = 16711935
    EXPECT_EQ(ColorTools::hexToRGBAInt32String("#00FF00"), "16711935");
}

TEST(ColorTools, hexToRGBAInt32String_OpaqueBlue)
{
    // Blue(0,0,255,255) → 0x0000FFFF = 65535
    EXPECT_EQ(ColorTools::hexToRGBAInt32String("#0000FF"), "65535");
}

TEST(ColorTools, hexToRGBAInt32String_OpaqueWhite)
{
    // White(255,255,255,255) → 0xFFFFFFFF = 4294967295
    EXPECT_EQ(ColorTools::hexToRGBAInt32String("#FFFFFF"), "4294967295");
}

TEST(ColorTools, hexToRGBAInt32String_TransparentBlack)
{
    // Black(0,0,0,0) with alpha 0 → 0x00000000 = 0
    EXPECT_EQ(ColorTools::hexToRGBAInt32String("#000000", 0), "0");
}

TEST(ColorTools, hexToRGBAInt32String_SemiTransparentARGB)
{
    // ARGB input #80FF0000 → detected as ARGB → RGBA(255,0,0,128)
    // 0xFF000080 = 4278190208
    EXPECT_EQ(ColorTools::hexToRGBAInt32String("#80FF0000"), "4278190208");
}

TEST(ColorTools, hexToRGBAInt32String_InvalidThrows)
{
    EXPECT_THROW(ColorTools::hexToRGBAInt32String(""), std::invalid_argument);
    EXPECT_THROW(ColorTools::hexToRGBAInt32String("#ZZ"), std::invalid_argument);
}