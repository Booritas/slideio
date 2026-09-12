// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <array>
#include <cstdint>
#include <iosfwd>
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
        /**@brief supplied by the caller through
         * ColorManagement::setSourceProfileOverride rather than found in the file.
         *
         * Distinct from Embedded on purpose: a supplied profile is a colorimetric
         * claim the caller makes about the scanner, not one the slide carries. A
         * pipeline auditing its corpus must be able to tell the two apart.*/
        Supplied,
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

    /**@brief what to do for a slide that embeds no ICC profile.
     *
     * Lives here rather than in slideio-transformer, alongside the rest of the
     * public colour vocabulary, so it can be named -- e.g. by a language
     * binding -- without pulling in ColorManagement's own header, which is
     * internal and drags in OpenCV.*/
    enum class MissingProfilePolicy
    {
        /**@brief treat the source as sRGB. Reads always succeed; the scene
         * reports ColorProfileSource::Assumed so absence stays visible.*/
        AssumeSRGB,
        /**@brief return decoded pixels untouched. Valid only for target sRGB.*/
        PassThrough,
        /**@brief throw at bind time. For pipelines that require real colorimetry.*/
        Fail,
    };

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

    /**@brief parsed ICC header facts. Populated by slideio-imagetools.
     *
     * present gates every other field, source included. Check it first: it is
     * false both when the scene carries no profile at all and when it carries
     * bytes that would not parse, and in the second case source still reads
     * Embedded -- it is copied from the profile the driver stamped, not derived
     * from a successful parse. present == false therefore means "no usable
     * colorimetry here", whatever source says.*/
    struct SLIDEIO_CORE_EXPORTS ColorProfileInfo
    {
        /**@brief true only if the bytes parsed as an ICC profile. See above:
         * nothing else in this struct is meaningful when it is false.*/
        bool present = false;
        ColorProfileSource source = ColorProfileSource::None;
        std::string description;
        std::string manufacturer;
        std::string model;
        std::string version;
        IccColorSpace dataSpace = IccColorSpace::Unknown;
        IccColorSpace connectionSpace = IccColorSpace::Unknown;
        RenderingIntent intent = RenderingIntent::RelativeColorimetric;
        /**@brief the profile's mediaWhitePointTag, as XYZ.
         *
         * NOT the scanner/device's native white in general: an ICC v4
         * profile (icc v4 requires this per ICC.1:2010 8.2.18) always
         * reports the PCS illuminant D50 (~0.9642, 1.0, 0.8249) here,
         * regardless of the device's actual white point -- the device's
         * native white for a v4 profile is recorded separately, via the
         * chromatic adaptation ("chad") tag, which this struct does not
         * expose. Only a v2 profile's mediaWhitePointTag is the device
         * white directly. Check version to know which case applies.*/
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
