// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/colorprofile.hpp"
#include <ostream>
#include <sstream>

using namespace slideio;

ColorProfile::ColorProfile(std::vector<uint8_t> iccBytes) : m_data(std::move(iccBytes))
{
    // A zero-length tag is not a profile: leave the source at None so a caller
    // cannot be told a profile exists when nothing was found.
    m_source = m_data.empty() ? ColorProfileSource::None : ColorProfileSource::Embedded;
}

std::string ColorProfileInfo::toString() const
{
    std::ostringstream os;
    os << "ColorProfileInfo(present=" << (present ? "true" : "false")
       << ", source=" << source;
    if (present) {
        os << ", description='" << description << "'"
           << ", space=" << dataSpace
           << ", pcs=" << connectionSpace
           << ", intent=" << intent
           << ", size=" << dataSize;
    }
    os << ")";
    return os.str();
}

std::ostream& slideio::operator << (std::ostream& os, ColorProfileSource source)
{
    switch (source) {
    case ColorProfileSource::None: os << "None"; break;
    case ColorProfileSource::Embedded: os << "Embedded"; break;
    case ColorProfileSource::Assumed: os << "Assumed"; break;
    default: os << "Unknown"; break;
    }
    return os;
}

std::ostream& slideio::operator << (std::ostream& os, IccColorSpace space)
{
    switch (space) {
    case IccColorSpace::Gray: os << "Gray"; break;
    case IccColorSpace::RGB: os << "RGB"; break;
    case IccColorSpace::CMYK: os << "CMYK"; break;
    case IccColorSpace::Lab: os << "Lab"; break;
    case IccColorSpace::XYZ: os << "XYZ"; break;
    case IccColorSpace::YCbCr: os << "YCbCr"; break;
    default: os << "Unknown"; break;
    }
    return os;
}

std::ostream& slideio::operator << (std::ostream& os, RenderingIntent intent)
{
    switch (intent) {
    case RenderingIntent::Perceptual: os << "Perceptual"; break;
    case RenderingIntent::RelativeColorimetric: os << "RelativeColorimetric"; break;
    case RenderingIntent::Saturation: os << "Saturation"; break;
    case RenderingIntent::AbsoluteColorimetric: os << "AbsoluteColorimetric"; break;
    default: os << "Unknown"; break;
    }
    return os;
}

std::ostream& slideio::operator << (std::ostream& os, ColorTarget target)
{
    switch (target) {
    case ColorTarget::sRGB: os << "sRGB"; break;
    case ColorTarget::LinearRGB: os << "LinearRGB"; break;
    case ColorTarget::Lab: os << "Lab"; break;
    case ColorTarget::XYZ: os << "XYZ"; break;
    default: os << "Unknown"; break;
    }
    return os;
}
