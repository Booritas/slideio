// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <algorithm>
#include "slideio/core/tools/color_tools.hpp"

#include "slideio/base/exceptions.hpp"

using namespace slideio;

namespace {

/**
 * Removes the '#' prefix from a hex color string if present.
 */
std::string stripHashPrefix(const std::string& hexColor) {
    if (!hexColor.empty() && hexColor[0] == '#') {
        return hexColor.substr(1);
    }
    return hexColor;
}

/**
 * Parses a hex string to a 32-bit unsigned integer.
 */
uint32_t parseHexToUint32(const std::string& hex) {
    uint32_t colorValue = 0;
    std::stringstream ss;
    ss << std::hex << hex;
    ss >> colorValue;
    return colorValue;
}

/**
 * Converts uint32_t to int32_t safely, avoiding implementation-defined behavior.
 */
int32_t uint32ToInt32(uint32_t u) {
    if (u <= 0x7FFFFFFFu) {
        return static_cast<int32_t>(u);
    }
    return static_cast<int32_t>(static_cast<int64_t>(u) - 0x100000000LL);
}

} // anonymous namespace


/**
 * Detects the color format from a hex color string.
 * Analyzes the string structure and potential alpha channel patterns.
 * 
 * For 8-digit hex colors, attempts to distinguish between RGBA and ARGB:
 * - If first 2 digits are 'FF' and others vary, likely ARGB (alpha first)
 * - If last 2 digits are 'FF' and others vary, likely RGBA (alpha last)
 * - Otherwise, defaults to ARGB (OME-TIFF standard)
 * 
 * @param hexColor The hex color string (with or without '#')
 * @return HexColorFormat enum indicating the detected format
 */
HexColorFormat ColorTools::detectHexColorFormat(const std::string& hexColor) {
    std::string hex = stripHashPrefix(hexColor);

    // Convert to uppercase for easier comparison
    std::transform(hex.begin(), hex.end(), hex.begin(), ::toupper);
    
    // Validate that all characters are hex digits
    for (char c : hex) {
        if (!std::isxdigit(c)) {
            return HexColorFormat::UNKNOWN;
        }
    }
    
    // Detect based on length
    switch (hex.length()) {
        case 6:
            return HexColorFormat::RGB;
        case 8: {
            // Need to distinguish between RGBA and ARGB
            // Check if first 2 digits suggest alpha channel (common: FF for fully opaque)
            std::string firstTwo = hex.substr(0, 2);
            std::string lastTwo = hex.substr(6, 2);
            
            // If first two are FF and last two are not, likely ARGB
            if (firstTwo == "FF" && lastTwo != "FF") {
                return HexColorFormat::ARGB;
            }
            // If last two are FF and first two are not, likely RGBA
            else if (lastTwo == "FF" && firstTwo != "FF") {
                return HexColorFormat::RGBA;
            }
            // If both or neither are FF, use heuristic:
            // ARGB is more common in scientific imaging (OME-TIFF)
            // But check if it looks like a valid color in either format
            else {
                // Default to ARGB for ambiguous cases (OME-TIFF standard)
                return HexColorFormat::ARGB;
            }
        }
            
        default:
            return HexColorFormat::UNKNOWN;
    }
}

/**
 * Returns a human-readable string representation of the color format.
 */
std::string ColorTools::HexColorFormatToString(HexColorFormat format) {
    switch (format) {
        case HexColorFormat::RGB:        return "RGB (#RRGGBB)";
        case HexColorFormat::RGBA:       return "RGBA (#RRGGBBAA)";
        case HexColorFormat::ARGB:       return "ARGB (#AARRGGBB)";
        case HexColorFormat::UNKNOWN:    return "Unknown";
        default:                      return "Invalid";
    }
}

/**
 * Converts a hex color string in ARGB format to RGBA array.
 * Input format: "#AARRGGBB" (e.g., "#FF0000FF" = Blue with full alpha)
 * Output: [R, G, B, A] array with values 0-255
 */
std::array<uint8_t, 4> ColorTools::hexARGBToRGBA(const std::string& hexColor) {
    const std::string hex = stripHashPrefix(hexColor);

    if (hex.length() != 8) {
        RAISE_RUNTIME_ERROR << "Invalid hex color format. Expected #AARRGGBB (8 hex digits)";
    }

    const uint32_t colorValue = parseHexToUint32(hex);

    return {
        static_cast<uint8_t>((colorValue >> 16) & 0xFF),  // Red
        static_cast<uint8_t>((colorValue >> 8) & 0xFF),   // Green
        static_cast<uint8_t>(colorValue & 0xFF),          // Blue
        static_cast<uint8_t>((colorValue >> 24) & 0xFF)   // Alpha
    };
}

/**
 * Converts a hex color string in RGBA format to RGBA array.
 * Input format: "#RRGGBBAA" (e.g., "#FF0000FF" = Red with full alpha)
 * Output: [R, G, B, A] array with values 0-255
 */
std::array<uint8_t, 4> ColorTools::hexRGBAToRGBA(const std::string& hexColor) {
    const std::string hex = stripHashPrefix(hexColor);

    if (hex.length() != 8) {
        RAISE_RUNTIME_ERROR << "Invalid hex color format. Expected #RRGGBBAA (8 hex digits)";
    }

    const uint32_t colorValue = parseHexToUint32(hex);

    return {
        static_cast<uint8_t>((colorValue >> 24) & 0xFF),  // Red
        static_cast<uint8_t>((colorValue >> 16) & 0xFF),  // Green
        static_cast<uint8_t>((colorValue >> 8) & 0xFF),   // Blue
        static_cast<uint8_t>(colorValue & 0xFF)           // Alpha
    };
}

/**
 * Converts a hex color string (RGB or RGBA) to RGBA array with optional alpha.
 * Input format: "#RRGGBB" or "#RRGGBBAA"
 * Output: [R, G, B, A] array with values 0-255
 * If no alpha is provided, defaults to 255 (fully opaque)
 */
std::array<uint8_t, 4> ColorTools::hexToRGBA(const std::string& hexColor, uint8_t defaultAlpha) {
    const std::string hex = stripHashPrefix(hexColor);

    if (hex.length() == 6) {
        const uint32_t colorValue = parseHexToUint32(hex);
        return {
            static_cast<uint8_t>((colorValue >> 16) & 0xFF),  // Red
            static_cast<uint8_t>((colorValue >> 8) & 0xFF),   // Green
            static_cast<uint8_t>(colorValue & 0xFF),          // Blue
            defaultAlpha
        };
    }
    if (hex.length() == 8) {
        return hexRGBAToRGBA(hexColor);
    }
    RAISE_RUNTIME_ERROR << "Invalid hex color format. Expected #RRGGBB or #RRGGBBAA";
}

/**
 * Converts RGBA array to RGB array (drops alpha channel).
 * Input: [R, G, B, A] array
 * Output: [R, G, B] array
 */
std::array<uint8_t, 3> ColorTools::RGBAToRGB(const std::array<uint8_t, 4>& rgba) {
    return {rgba[0], rgba[1], rgba[2]};
}

/**
 * Converts RGBA array back to hex string in ARGB format.
 * Input: [R, G, B, A] array
 * Output: "#AARRGGBB" string
 */
std::string ColorTools::RGBAToHexARGB(const std::array<uint8_t, 4>& rgba) {
    std::stringstream ss;
    ss << "#" 
       << std::hex << std::uppercase << std::setfill('0')
       << std::setw(2) << static_cast<int>(rgba[3])  // Alpha
       << std::setw(2) << static_cast<int>(rgba[0])  // Red
       << std::setw(2) << static_cast<int>(rgba[1])  // Green
       << std::setw(2) << static_cast<int>(rgba[2]); // Blue
    return ss.str();
}

/**
 * Converts RGBA array back to hex string in RGBA format.
 * Input: [R, G, B, A] array
 * Output: "#RRGGBBAA" string
 */
std::string ColorTools::RGBAToHexRGBA(const std::array<uint8_t, 4>& rgba) {
    std::stringstream ss;
    ss << "#" 
       << std::hex << std::uppercase << std::setfill('0')
       << std::setw(2) << static_cast<int>(rgba[0])  // Red
       << std::setw(2) << static_cast<int>(rgba[1])  // Green
       << std::setw(2) << static_cast<int>(rgba[2])  // Blue
       << std::setw(2) << static_cast<int>(rgba[3]); // Alpha
    return ss.str();
}


/**
 * Smart color converter that automatically detects the format and converts to RGBA.
 * Supports: RGB, RGBA, ARGB, and shorthand formats.
 * 
 * @param hexColor The hex color string
 * @param defaultAlpha Default alpha value if not present (default: 255)
 * @return RGBA array [R, G, B, A]
 */
std::array<uint8_t, 4> ColorTools::smartHexToRGBA(const std::string& hexColor, uint8_t defaultAlpha) {
    const HexColorFormat format = detectHexColorFormat(hexColor);

    switch (format) {
        case HexColorFormat::RGB:
            return hexToRGBA(hexColor, defaultAlpha);
        case HexColorFormat::RGBA:
            return hexRGBAToRGBA(hexColor);
        case HexColorFormat::ARGB:
            return hexARGBToRGBA(hexColor);
        default:
            RAISE_RUNTIME_ERROR << "Unable to detect valid color format from: " << hexColor;
    }
}

/**
 * Converts a hex color string to a 32-bit RGBA integer and returns it as a
 * decimal string. Auto-detects the input format (RGB, RGBA, ARGB).
 *
 * The 32-bit integer is packed as 0xAARRGGBB or 0xRRGGBB if alpha value is not set.
 *
 * @param hexColor The hex color string
 * @return Decimal string representation of the 32-bit RGBA/RGB value
 */
std::string ColorTools::hexToInt32String(const std::string& hexColor) {
    const HexColorFormat format = detectHexColorFormat(hexColor);

    if (format == HexColorFormat::UNKNOWN) {
        RAISE_RUNTIME_ERROR << "Unknown color representation: " << hexColor;
    }

    constexpr uint8_t defaultAlpha = 0;
    const auto rgba = smartHexToRGBA(hexColor, defaultAlpha);

    uint32_t packed = 0;
    if (format == HexColorFormat::RGB) {
        packed = (static_cast<uint32_t>(rgba[0]) << 16)
               | (static_cast<uint32_t>(rgba[1]) << 8)
               | static_cast<uint32_t>(rgba[2]);
    } else {
        packed = (static_cast<uint32_t>(rgba[3]) << 24)
               | (static_cast<uint32_t>(rgba[0]) << 16)
               | (static_cast<uint32_t>(rgba[1]) << 8)
               | static_cast<uint32_t>(rgba[2]);
    }

    return std::to_string(uint32ToInt32(packed));
}
