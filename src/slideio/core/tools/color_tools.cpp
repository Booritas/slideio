#include <string>
#include <vector>
#include <stdexcept>
#include <array>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <algorithm>

/**
 * Enum representing different color format types
 */
enum class ColorFormat {
    UNKNOWN,
    RGB,        // #RRGGBB (6 hex digits)
    RGBA,       // #RRGGBBAA (8 hex digits, standard web format)
    ARGB,       // #AARRGGBB (8 hex digits, OME-TIFF format)
    SHORT_RGB,  // #RGB (3 hex digits, shorthand)
    SHORT_RGBA  // #RGBA (4 hex digits, shorthand)
};

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
 * @return ColorFormat enum indicating the detected format
 */
ColorFormat detectColorFormat(const std::string& hexColor) {
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
            return ColorFormat::UNKNOWN;
        }
    }
    
    // Detect based on length
    switch (hex.length()) {
        case 3:
            return ColorFormat::SHORT_RGB;
            
        case 4:
            return ColorFormat::SHORT_RGBA;
            
        case 6:
            return ColorFormat::RGB;
            
        case 8: {
            // Need to distinguish between RGBA and ARGB
            // Check if first 2 digits suggest alpha channel (common: FF for fully opaque)
            std::string firstTwo = hex.substr(0, 2);
            std::string lastTwo = hex.substr(6, 2);
            
            // If first two are FF and last two are not, likely ARGB
            if (firstTwo == "FF" && lastTwo != "FF") {
                return ColorFormat::ARGB;
            }
            // If last two are FF and first two are not, likely RGBA
            else if (lastTwo == "FF" && firstTwo != "FF") {
                return ColorFormat::RGBA;
            }
            // If both or neither are FF, use heuristic:
            // ARGB is more common in scientific imaging (OME-TIFF)
            // But check if it looks like a valid color in either format
            else {
                // Default to ARGB for ambiguous cases (OME-TIFF standard)
                return ColorFormat::ARGB;
            }
        }
            
        default:
            return ColorFormat::UNKNOWN;
    }
}

/**
 * Returns a human-readable string representation of the color format.
 */
std::string colorFormatToString(ColorFormat format) {
    switch (format) {
        case ColorFormat::RGB:        return "RGB (#RRGGBB)";
        case ColorFormat::RGBA:       return "RGBA (#RRGGBBAA)";
        case ColorFormat::ARGB:       return "ARGB (#AARRGGBB)";
        case ColorFormat::SHORT_RGB:  return "Short RGB (#RGB)";
        case ColorFormat::SHORT_RGBA: return "Short RGBA (#RGBA)";
        case ColorFormat::UNKNOWN:    return "Unknown";
        default:                      return "Invalid";
    }
}

/**
 * Converts a hex color string in ARGB format to RGBA array.
 * Input format: "#AARRGGBB" (e.g., "#FF0000FF" = Blue with full alpha)
 * Output: [R, G, B, A] array with values 0-255
 */
std::array<uint8_t, 4> hexARGBToRGBA(const std::string& hexColor) {
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
std::array<uint8_t, 4> hexRGBAToRGBA(const std::string& hexColor) {
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
std::array<uint8_t, 4> hexToRGBA(const std::string& hexColor, uint8_t defaultAlpha = 255) {
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
std::array<uint8_t, 3> RGBAToRGB(const std::array<uint8_t, 4>& rgba) {
    return {rgba[0], rgba[1], rgba[2]};
}

/**
 * Converts RGBA array back to hex string in ARGB format.
 * Input: [R, G, B, A] array
 * Output: "#AARRGGBB" string
 */
std::string RGBAToHexARGB(const std::array<uint8_t, 4>& rgba) {
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
std::string RGBAToHexRGBA(const std::array<uint8_t, 4>& rgba) {
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
std::string expandShortHex(const std::string& hexColor) {
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
std::array<uint8_t, 4> smartHexToRGBA(const std::string& hexColor, uint8_t defaultAlpha = 255) {
    ColorFormat format = detectColorFormat(hexColor);
    
    std::string processedHex = hexColor;
    
    // Expand shorthand formats first
    if (format == ColorFormat::SHORT_RGB || format == ColorFormat::SHORT_RGBA) {
        processedHex = expandShortHex(hexColor);
        format = detectColorFormat(processedHex);
    }
    
    // Convert based on detected format
    switch (format) {
        case ColorFormat::RGB:
            return hexToRGBA(processedHex, defaultAlpha);
            
        case ColorFormat::RGBA:
            return hexRGBAToRGBA(processedHex);
            
        case ColorFormat::ARGB:
            return hexARGBToRGBA(processedHex);
            
        default:
            throw std::invalid_argument("Unable to detect valid color format from: " + hexColor);
    }
}

// Example usage and test function
#include <iostream>

void testColorConversions() {
    std::cout << "=== Color Conversion Tests ===" << std::endl << std::endl;
    
    // Test format detection
    std::cout << "=== Format Detection Tests ===" << std::endl << std::endl;
    
    std::vector<std::string> testColors = {
        "#FF0000FF",    // Could be ARGB (Blue) or RGBA (Red with alpha)
        "#00FF00",      // RGB Green
        "#ABC",         // Short RGB
        "#ABCD",        // Short RGBA
        "#FF00FF00",    // ARGB Green (OME-TIFF)
        "#FFFFFF00",    // ARGB Yellow
        "#12345678"     // Ambiguous 8-digit
    };
    
    for (const auto& color : testColors) {
        ColorFormat format = detectColorFormat(color);
        std::cout << "Color: " << color << std::endl;
        std::cout << "  Detected format: " << colorFormatToString(format) << std::endl;
        std::cout << std::endl;
    }
    
    // Test smart conversion
    std::cout << "=== Smart Conversion Tests ===" << std::endl << std::endl;
    
    std::cout << "Test 1: Short RGB (#F00 -> Red)" << std::endl;
    auto rgba1 = smartHexToRGBA("#F00");
    std::cout << "Input:  #F00" << std::endl;
    std::cout << "Output: [" 
              << static_cast<int>(rgba1[0]) << ", "
              << static_cast<int>(rgba1[1]) << ", "
              << static_cast<int>(rgba1[2]) << ", "
              << static_cast<int>(rgba1[3]) << "]" << std::endl;
    std::cout << "Expected: [255, 0, 0, 255]" << std::endl << std::endl;
    
    std::cout << "Test 2: RGB (#00FF00 -> Green)" << std::endl;
    auto rgba2 = smartHexToRGBA("#00FF00");
    std::cout << "Input:  #00FF00" << std::endl;
    std::cout << "Output: [" 
              << static_cast<int>(rgba2[0]) << ", "
              << static_cast<int>(rgba2[1]) << ", "
              << static_cast<int>(rgba2[2]) << ", "
              << static_cast<int>(rgba2[3]) << "]" << std::endl;
    std::cout << "Expected: [0, 255, 0, 255]" << std::endl << std::endl;
    
    std::cout << "Test 3: ARGB (#FF0000FF -> Blue, OME-TIFF format)" << std::endl;
    auto rgba3 = smartHexToRGBA("#FF0000FF");
    std::cout << "Input:  #FF0000FF (ARGB)" << std::endl;
    std::cout << "Output: [" 
              << static_cast<int>(rgba3[0]) << ", "
              << static_cast<int>(rgba3[1]) << ", "
              << static_cast<int>(rgba3[2]) << ", "
              << static_cast<int>(rgba3[3]) << "]" << std::endl;
    std::cout << "Expected: [0, 0, 255, 255]" << std::endl << std::endl;
    
    // Test ARGB to RGBA (OME-TIFF format)
    std::cout << "=== Legacy Function Tests ===" << std::endl << std::endl;
    
    std::cout << "Test 4: ARGB to RGBA" << std::endl;
    std::string argbBlue = "#FF0000FF";  // Alpha=FF, Red=00, Green=00, Blue=FF
    auto rgbaBlue = hexARGBToRGBA(argbBlue);
    std::cout << "Input (ARGB):  " << argbBlue << std::endl;
    std::cout << "Output (RGBA): [" 
              << static_cast<int>(rgbaBlue[0]) << ", "
              << static_cast<int>(rgbaBlue[1]) << ", "
              << static_cast<int>(rgbaBlue[2]) << ", "
              << static_cast<int>(rgbaBlue[3]) << "]" << std::endl;
    std::cout << "Expected:      [0, 0, 255, 255]" << std::endl << std::endl;
    
    // Test RGBA to RGBA
    std::cout << "Test 5: RGBA to RGBA" << std::endl;
    // Test RGBA to RGBA
    std::cout << "Test 5: RGBA to RGBA" << std::endl;
    std::string rgbaRed = "#FF0000FF";  // Red=FF, Green=00, Blue=00, Alpha=FF
    auto rgbaRed_out = hexRGBAToRGBA(rgbaRed);
    std::cout << "Input (RGBA):  " << rgbaRed << std::endl;
    std::cout << "Output (RGBA): [" 
              << static_cast<int>(rgbaRed_out[0]) << ", "
              << static_cast<int>(rgbaRed_out[1]) << ", "
              << static_cast<int>(rgbaRed_out[2]) << ", "
              << static_cast<int>(rgbaRed_out[3]) << "]" << std::endl;
    std::cout << "Expected:      [255, 0, 0, 255]" << std::endl << std::endl;
    
    // Test RGB to RGBA (with default alpha)
    std::cout << "Test 6: RGB to RGBA (default alpha)" << std::endl;
    std::string rgbGreen = "#00FF00";
    auto rgbaGreen = hexToRGBA(rgbGreen);
    std::cout << "Input (RGB):   " << rgbGreen << std::endl;
    std::cout << "Output (RGBA): [" 
              << static_cast<int>(rgbaGreen[0]) << ", "
              << static_cast<int>(rgbaGreen[1]) << ", "
              << static_cast<int>(rgbaGreen[2]) << ", "
              << static_cast<int>(rgbaGreen[3]) << "]" << std::endl;
    std::cout << "Expected:      [0, 255, 0, 255]" << std::endl << std::endl;
    
    // Test RGBA to RGB (drop alpha)
    std::cout << "Test 7: RGBA to RGB (drop alpha)" << std::endl;
    // Test RGBA to RGB (drop alpha)
    std::cout << "Test 7: RGBA to RGB (drop alpha)" << std::endl;
    std::array<uint8_t, 4> rgba = {255, 128, 0, 255};
    auto rgb = RGBAToRGB(rgba);
    std::cout << "Input (RGBA):  [" 
              << static_cast<int>(rgba[0]) << ", "
              << static_cast<int>(rgba[1]) << ", "
              << static_cast<int>(rgba[2]) << ", "
              << static_cast<int>(rgba[3]) << "]" << std::endl;
    std::cout << "Output (RGB):  [" 
              << static_cast<int>(rgb[0]) << ", "
              << static_cast<int>(rgb[1]) << ", "
              << static_cast<int>(rgb[2]) << "]" << std::endl;
    std::cout << "Expected:      [255, 128, 0]" << std::endl << std::endl;
    
    // Test round-trip conversion
    std::cout << "Test 8: Round-trip ARGB -> RGBA -> ARGB" << std::endl;
    std::string original = "#FF00FFFF";  // Cyan with full alpha in ARGB
    auto rgba_converted = hexARGBToRGBA(original);
    auto backToHex = RGBAToHexARGB(rgba_converted);
    std::cout << "Original:      " << original << std::endl;
    std::cout << "After round-trip: " << backToHex << std::endl;
    std::cout << "Match: " << (original == backToHex ? "YES" : "NO") << std::endl << std::endl;
    
    // Test OME-TIFF color examples from your file
    std::cout << "=== OME-TIFF Color Examples ===" << std::endl << std::endl;
    
    std::vector<std::pair<std::string, std::string>> omeTiffColors = {
        {"DAPI", "#FF0000FF"},
        {"FITC", "#FF00FF00"},
        {"DsRed", "#FFFFFF00"},
        {"Cy5", "#FFFF0000"},
        {"Cy7", "#FF00FFFF"}
    };
    
    for (const auto& [name, hexColor] : omeTiffColors) {
        auto rgba = hexARGBToRGBA(hexColor);
        auto rgb = RGBAToRGB(rgba);
        std::cout << name << " (" << hexColor << "):" << std::endl;
        std::cout << "  RGBA: [" 
                  << static_cast<int>(rgba[0]) << ", "
                  << static_cast<int>(rgba[1]) << ", "
                  << static_cast<int>(rgba[2]) << ", "
                  << static_cast<int>(rgba[3]) << "]" << std::endl;
        std::cout << "  RGB:  [" 
                  << static_cast<int>(rgb[0]) << ", "
                  << static_cast<int>(rgb[1]) << ", "
                  << static_cast<int>(rgb[2]) << "]" << std::endl;
    }
}

int main() {
    try {
        testColorConversions();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
