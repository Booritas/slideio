// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
#include <array>
#include <string>

namespace slideio {
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

    class ColorTools {
    public:
        static ColorFormat detectColorFormat(const std::string& hexColor);
        static std::string colorFormatToString(ColorFormat format);
        static std::array<uint8_t, 4> hexARGBToRGBA(const std::string& hexColor);
        static std::array<uint8_t, 4> hexRGBAToRGBA(const std::string& hexColor);
        static std::array<uint8_t, 4> hexToRGBA(const std::string& hexColor, uint8_t defaultAlpha = 255);
        static std::array<uint8_t, 3> RGBAToRGB(const std::array<uint8_t, 4>& rgba);
        static std::string RGBAToHexARGB(const std::array<uint8_t, 4>& rgba);
        static std::string expandShortHex(const std::string& hexColor);
        static std::array<uint8_t, 4> smartHexToRGBA(const std::string& hexColor, uint8_t defaultAlpha = 255);
        static std::string RGBAToHexRGBA(const std::array<uint8_t, 4>& rgba);
    };
}