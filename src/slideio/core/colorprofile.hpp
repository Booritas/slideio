// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "slideio/core/slideio_core_def.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    /**@brief where a scene's colour profile came from */
    enum class ColorProfileSource
    {
        /**@brief the file carries no profile*/
        None,
        /**@brief a real ICC profile read out of the file*/
        Embedded,
        /**@brief none embedded; sRGB assumed under MissingProfilePolicy*/
        Assumed,
    };

    /**@brief colour space of ICC profile data.
     *
     * Distinct from slideio::ColorSpace in the transformer, which describes
     * OpenCV conversions and is unrelated to ICC.*/
    enum class IccColorSpace { Unknown, Gray, RGB, CMYK, Lab, XYZ, YCbCr };

    /**@brief ICC rendering intent*/
    enum class RenderingIntent
    {
        Perceptual, RelativeColorimetric, Saturation, AbsoluteColorimetric
    };

    /**@brief device-independent space a scene's pixels may be converted into*/
    enum class ColorTarget { sRGB, LinearRGB, Lab, XYZ };

    /**@brief raw ICC profile bytes as found in a slide.
     *
     * A byte container only: it does not parse or validate its contents. Use
     * Scene::getColorProfileInfo() for the parsed header, or hand getData() to
     * an external colour management system. Parsing lives in slideio-imagetools
     * because it needs lcms2, which neither this module nor any driver may see.*/
    class SLIDEIO_CORE_EXPORTS ColorProfile
    {
    public:
        ColorProfile() = default;
        explicit ColorProfile(std::vector<uint8_t> iccBytes);
        bool isEmpty() const { return m_data.empty(); }
        ColorProfileSource getSource() const { return m_source; }
        void setSource(ColorProfileSource source) { m_source = source; }
        const std::vector<uint8_t>& getData() const { return m_data; }
        size_t getSize() const { return m_data.size(); }
    private:
        std::vector<uint8_t> m_data;
        ColorProfileSource m_source = ColorProfileSource::None;
    };

    /**@brief parsed ICC header facts. Populated by slideio-imagetools.*/
    struct SLIDEIO_CORE_EXPORTS ColorProfileInfo
    {
        bool present = false;
        ColorProfileSource source = ColorProfileSource::None;
        std::string description;
        std::string manufacturer;
        std::string model;
        std::string version;
        IccColorSpace dataSpace = IccColorSpace::Unknown;
        IccColorSpace connectionSpace = IccColorSpace::Unknown;
        RenderingIntent intent = RenderingIntent::RelativeColorimetric;
        std::array<double, 3> whitePoint{0.0, 0.0, 0.0};
        size_t dataSize = 0;
        std::string toString() const;
    };

    SLIDEIO_CORE_EXPORTS std::ostream& operator << (std::ostream& os, ColorProfileSource source);
    SLIDEIO_CORE_EXPORTS std::ostream& operator << (std::ostream& os, IccColorSpace space);
    SLIDEIO_CORE_EXPORTS std::ostream& operator << (std::ostream& os, RenderingIntent intent);
    SLIDEIO_CORE_EXPORTS std::ostream& operator << (std::ostream& os, ColorTarget target);
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
