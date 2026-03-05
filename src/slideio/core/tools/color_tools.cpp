// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
#include <vector>
#include <stdexcept>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <algorithm>
#include "slideio/core/tools/color_tools.hpp"
using namespace slideio;


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
    // Remove '#' if present and convert to uppercase
    std::string hex = hexColor;
    if (!hex.empty() && hex[0] == '#') {
        hex = hex.substr(1);
    }
    
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
        case 3:
            return HexColorFormat::SHORT_RGB;
            
        case 4:
            return HexColorFormat::SHORT_RGBA;
            
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
        case HexColorFormat::SHORT_RGB:  return "Short RGB (#RGB)";
        case HexColorFormat::SHORT_RGBA: return "Short RGBA (#RGBA)";
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
    // Remove '#' if present
    std::string hex = hexColor;
    if (!hex.empty() && hex[0] == '#') {
        hex = hex.substr(1);
    }
    
    // Validate length (must be 8 characters for ARGB)
    if (hex.length() != 8) {
        throw std::invalid_argument("Invalid hex color format. Expected #AARRGGBB (8 hex digits)");
    }
    
    // Parse hex string to integer
    uint32_t colorValue;
    std::stringstream ss;
    ss << std::hex << hex;
    ss >> colorValue;
    
    // Extract ARGB components
    uint8_t alpha = (colorValue >> 24) & 0xFF;
    uint8_t red   = (colorValue >> 16) & 0xFF;
    uint8_t green = (colorValue >> 8)  & 0xFF;
    uint8_t blue  = colorValue & 0xFF;
    
    // Return as RGBA array
    return {red, green, blue, alpha};
}

/**
 * Converts a hex color string in RGBA format to RGBA array.
 * Input format: "#RRGGBBAA" (e.g., "#FF0000FF" = Red with full alpha)
 * Output: [R, G, B, A] array with values 0-255
 */
std::array<uint8_t, 4> ColorTools::hexRGBAToRGBA(const std::string& hexColor) {
    // Remove '#' if present
    std::string hex = hexColor;
    if (!hex.empty() && hex[0] == '#') {
        hex = hex.substr(1);
    }
    
    // Validate length (must be 8 characters for RGBA)
    if (hex.length() != 8) {
        throw std::invalid_argument("Invalid hex color format. Expected #RRGGBBAA (8 hex digits)");
    }
    
    // Parse hex string to integer
    uint32_t colorValue;
    std::stringstream ss;
    ss << std::hex << hex;
    ss >> colorValue;
    
    // Extract RGBA components
    uint8_t red   = (colorValue >> 24) & 0xFF;
    uint8_t green = (colorValue >> 16) & 0xFF;
    uint8_t blue  = (colorValue >> 8)  & 0xFF;
    uint8_t alpha = colorValue & 0xFF;
    
    // Return as RGBA array
    return {red, green, blue, alpha};
}

/**
 * Converts a hex color string (RGB or RGBA) to RGBA array with optional alpha.
 * Input format: "#RRGGBB" or "#RRGGBBAA"
 * Output: [R, G, B, A] array with values 0-255
 * If no alpha is provided, defaults to 255 (fully opaque)
 */
std::array<uint8_t, 4> ColorTools::hexToRGBA(const std::string& hexColor, uint8_t defaultAlpha) {
    // Remove '#' if present
    std::string hex = hexColor;
    if (!hex.empty() && hex[0] == '#') {
        hex = hex.substr(1);
    }
    
    // Parse based on length
    if (hex.length() == 6) {
        // RGB format
        uint32_t colorValue;
        std::stringstream ss;
        ss << std::hex << hex;
        ss >> colorValue;
        
        uint8_t red   = (colorValue >> 16) & 0xFF;
        uint8_t green = (colorValue >> 8)  & 0xFF;
        uint8_t blue  = colorValue & 0xFF;
        
        return {red, green, blue, defaultAlpha};
    } 
    else if (hex.length() == 8) {
        // RGBA format
        return hexRGBAToRGBA("#" + hex);
    } 
    else {
        throw std::invalid_argument("Invalid hex color format. Expected #RRGGBB or #RRGGBBAA");
    }
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
 * Expands short hex format to full format.
 * #RGB -> #RRGGBB
 * #RGBA -> #RRGGBBAA
 */
std::string ColorTools::expandShortHex(const std::string& hexColor) {
    std::string hex = hexColor;
    if (!hex.empty() && hex[0] == '#') {
        hex = hex.substr(1);
    }
    
    if (hex.length() == 3) {
        // RGB: #ABC -> #AABBCC
        std::stringstream ss;
        ss << "#";
        for (char c : hex) {
            ss << c << c;
        }
        return ss.str();
    } else if (hex.length() == 4) {
        // RGBA: #ABCD -> #AABBCCDD
        std::stringstream ss;
        ss << "#";
        for (char c : hex) {
            ss << c << c;
        }
        return ss.str();
    }
    
    return hexColor;
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
    HexColorFormat format = detectHexColorFormat(hexColor);
    
    std::string processedHex = hexColor;
    
    // Expand shorthand formats first
    if (format == HexColorFormat::SHORT_RGB || format == HexColorFormat::SHORT_RGBA) {
        processedHex = expandShortHex(hexColor);
        format = detectHexColorFormat(processedHex);
    }
    
    // Convert based on detected format
    switch (format) {
        case HexColorFormat::RGB:
            return hexToRGBA(processedHex, defaultAlpha);
            
        case HexColorFormat::RGBA:
            return hexRGBAToRGBA(processedHex);
            
        case HexColorFormat::ARGB:
            return hexARGBToRGBA(processedHex);
            
        default:
            throw std::invalid_argument("Unable to detect valid color format from: " + hexColor);
    }
}

/**
 * Converts a hex color string to a 32-bit RGBA integer and returns it as a
 * decimal string. Auto-detects the input format (RGB, RGBA, ARGB, shorthand).
 *
 * The 32-bit integer is packed as 0xRRGGBBAA.
 *
 * @param hexColor The hex color string
 * @param defaultAlpha Default alpha value if not present (default: 255)
 * @return Decimal string representation of the 32-bit RGBA value
 */
std::string ColorTools::hexToRGBAInt32String(const std::string& hexColor, uint8_t defaultAlpha) {
    auto rgba = smartHexToRGBA(hexColor, defaultAlpha);
    uint32_t packed = (static_cast<uint32_t>(rgba[0]) << 24)
                    | (static_cast<uint32_t>(rgba[1]) << 16)
                    | (static_cast<uint32_t>(rgba[2]) << 8)
                    |  static_cast<uint32_t>(rgba[3]);
    return std::to_string(packed);
}