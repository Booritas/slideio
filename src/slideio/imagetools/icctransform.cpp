// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/imagetools/icctransform.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/log.hpp"
#include <cstdio>
#include <lcms2.h>

using namespace slideio;

namespace
{
    IccColorSpace toIccColorSpace(cmsColorSpaceSignature sig)
    {
        switch (sig) {
        case cmsSigGrayData: return IccColorSpace::Gray;
        case cmsSigRgbData: return IccColorSpace::RGB;
        case cmsSigCmykData: return IccColorSpace::CMYK;
        case cmsSigLabData: return IccColorSpace::Lab;
        case cmsSigXYZData: return IccColorSpace::XYZ;
        case cmsSigYCbCrData: return IccColorSpace::YCbCr;
        default: return IccColorSpace::Unknown;
        }
    }

    RenderingIntent toRenderingIntent(cmsUInt32Number intent)
    {
        switch (intent) {
        case INTENT_PERCEPTUAL: return RenderingIntent::Perceptual;
        case INTENT_SATURATION: return RenderingIntent::Saturation;
        case INTENT_ABSOLUTE_COLORIMETRIC: return RenderingIntent::AbsoluteColorimetric;
        default: return RenderingIntent::RelativeColorimetric;
        }
    }

    std::string readProfileText(cmsHPROFILE handle, cmsInfoType type)
    {
        char buffer[512] = {0};
        const cmsUInt32Number size =
            cmsGetProfileInfoASCII(handle, type, "en", "US", buffer, sizeof(buffer) - 1);
        return size > 0 ? std::string(buffer) : std::string();
    }

    // cmsGetProfileVersion() encodes major.minor.bugfix as a single decimal
    // (its own documented convention: minor in the tenths place, bugfix in
    // the hundredths place -- e.g. 4.3 for ICC v4.3.0.0, 2.1 for v2.1.0.0).
    // A previous version of this code truncated that double with
    // static_cast<cmsUInt32Number>(...) before stringifying it, which
    // silently dropped minor and bugfix for every real-world profile (v2.1,
    // v4.2, v4.3, v4.4 all became just "2" or "4"). Format both digits and
    // trim only a non-informative trailing zero in the hundredths place,
    // since bugfix is 0 for almost every profile in the wild.
    std::string formatIccVersion(double version)
    {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.2f", version);
        std::string text(buffer);
        if (text.size() > 1 && text.back() == '0') {
            text.pop_back();
        }
        return text;
    }
}

ColorProfileInfo IccTransform::describe(const ColorProfile& profile)
{
    ColorProfileInfo info;
    info.source = profile.getSource();
    info.dataSize = profile.getSize();
    if (profile.isEmpty()) {
        return info;
    }
    cmsHPROFILE handle = cmsOpenProfileFromMem(profile.getData().data(),
                                               static_cast<cmsUInt32Number>(profile.getSize()));
    if (!handle) {
        SLIDEIO_LOG(WARNING) << "IccTransform: cannot parse an ICC profile of "
                             << profile.getSize() << " bytes; treating it as absent";
        return info;
    }
    info.present = true;
    info.description = readProfileText(handle, cmsInfoDescription);
    info.manufacturer = readProfileText(handle, cmsInfoManufacturer);
    info.model = readProfileText(handle, cmsInfoModel);
    info.dataSpace = toIccColorSpace(cmsGetColorSpace(handle));
    info.connectionSpace = toIccColorSpace(cmsGetPCS(handle));
    info.intent = toRenderingIntent(cmsGetHeaderRenderingIntent(handle));

    info.version = formatIccVersion(cmsGetProfileVersion(handle));

    if (const cmsCIEXYZ* wp = static_cast<const cmsCIEXYZ*>(
            cmsReadTag(handle, cmsSigMediaWhitePointTag))) {
        info.whitePoint = {wp->X, wp->Y, wp->Z};
    }
    cmsCloseProfile(handle);
    return info;
}

ColorProfile IccTransform::createSRGBProfile()
{
    cmsHPROFILE handle = cmsCreate_sRGBProfile();
    if (!handle) {
        RAISE_RUNTIME_ERROR << "IccTransform: lcms2 failed to create an sRGB profile";
    }
    cmsUInt32Number size = 0;
    if (!cmsSaveProfileToMem(handle, nullptr, &size) || size == 0) {
        cmsCloseProfile(handle);
        RAISE_RUNTIME_ERROR << "IccTransform: cannot measure the synthetic sRGB profile";
    }
    std::vector<uint8_t> bytes(size);
    if (!cmsSaveProfileToMem(handle, bytes.data(), &size)) {
        cmsCloseProfile(handle);
        RAISE_RUNTIME_ERROR << "IccTransform: cannot serialise the synthetic sRGB profile";
    }
    cmsCloseProfile(handle);
    ColorProfile profile(std::move(bytes));
    profile.setSource(ColorProfileSource::Assumed);
    return profile;
}

IccTransform::~IccTransform()
{
    if (m_transform) {
        cmsDeleteTransform(static_cast<cmsHTRANSFORM>(m_transform));
    }
}

namespace
{
    cmsHPROFILE createTargetProfile(ColorTarget target)
    {
        switch (target) {
        case ColorTarget::sRGB:
            return cmsCreate_sRGBProfile();
        case ColorTarget::Lab:
            return cmsCreateLab4Profile(nullptr);
        case ColorTarget::XYZ:
            return cmsCreateXYZProfile();
        case ColorTarget::LinearRGB: {
            // sRGB primaries and white point with a gamma-1.0 tone curve.
            cmsCIExyY whitePoint{0.3127, 0.3290, 1.0};
            cmsCIExyYTRIPLE primaries{{0.6400, 0.3300, 1.0},
                                      {0.3000, 0.6000, 1.0},
                                      {0.1500, 0.0600, 1.0}};
            cmsToneCurve* linear = cmsBuildGamma(nullptr, 1.0);
            cmsToneCurve* curves[3] = {linear, linear, linear};
            cmsHPROFILE profile = cmsCreateRGBProfile(&whitePoint, &primaries, curves);
            cmsFreeToneCurve(linear);
            return profile;
        }
        default:
            return nullptr;
        }
    }

    cmsUInt32Number sourceFormat(DataType type)
    {
        // TYPE_RGB_*, never TYPE_BGR_*: slideio buffers are in scene channel
        // order, which is RGB. OpenCV's BGR convention does not apply here.
        switch (type) {
        case DataType::DT_Byte: return TYPE_RGB_8;
        case DataType::DT_UInt16: return TYPE_RGB_16;
        default: return 0;
        }
    }

    cmsUInt32Number targetFormat(ColorTarget target, DataType sourceType)
    {
        switch (target) {
        case ColorTarget::sRGB: return sourceFormat(sourceType);
        case ColorTarget::Lab: return TYPE_Lab_FLT;
        case ColorTarget::XYZ: return TYPE_XYZ_FLT;
        case ColorTarget::LinearRGB: return TYPE_RGB_FLT;
        default: return 0;
        }
    }

    cmsUInt32Number toLcmsIntent(RenderingIntent intent)
    {
        switch (intent) {
        case RenderingIntent::Perceptual: return INTENT_PERCEPTUAL;
        case RenderingIntent::Saturation: return INTENT_SATURATION;
        case RenderingIntent::AbsoluteColorimetric: return INTENT_ABSOLUTE_COLORIMETRIC;
        default: return INTENT_RELATIVE_COLORIMETRIC;
        }
    }
}

IccTransform::IccTransform(const ColorProfile& source, ColorTarget target,
                           RenderingIntent intent, bool blackPointCompensation,
                           DataType sourceType)
{
    const cmsUInt32Number srcFormat = sourceFormat(sourceType);
    if (srcFormat == 0) {
        RAISE_RUNTIME_ERROR << "IccTransform: unsupported source data type " << sourceType
                            << "; only DT_Byte and DT_UInt16 are colorimetric";
    }
    if (source.isEmpty()) {
        RAISE_RUNTIME_ERROR << "IccTransform: an empty source profile cannot be converted";
    }

    cmsHPROFILE srcProfile = cmsOpenProfileFromMem(
        source.getData().data(), static_cast<cmsUInt32Number>(source.getSize()));
    if (!srcProfile) {
        RAISE_RUNTIME_ERROR << "IccTransform: cannot parse the source ICC profile ("
                            << source.getSize() << " bytes)";
    }
    cmsHPROFILE dstProfile = createTargetProfile(target);
    if (!dstProfile) {
        cmsCloseProfile(srcProfile);
        RAISE_RUNTIME_ERROR << "IccTransform: cannot create a profile for target " << target;
    }

    const cmsUInt32Number flags =
        blackPointCompensation ? cmsFLAGS_BLACKPOINTCOMPENSATION : 0;
    m_transform = cmsCreateTransform(srcProfile, srcFormat, dstProfile,
                                     targetFormat(target, sourceType),
                                     toLcmsIntent(intent), flags);
    cmsCloseProfile(srcProfile);
    cmsCloseProfile(dstProfile);
    if (!m_transform) {
        RAISE_RUNTIME_ERROR << "IccTransform: lcms2 could not build a transform to " << target;
    }
    m_outputType = (target == ColorTarget::sRGB) ? sourceType : DataType::DT_Float32;
}

void IccTransform::apply(const cv::Mat& src, cv::OutputArray dst) const
{
    if (src.channels() != 3) {
        RAISE_RUNTIME_ERROR << "IccTransform: expected 3 channels, received " << src.channels();
    }
    if (!src.isContinuous()) {
        RAISE_RUNTIME_ERROR << "IccTransform: expected a continuous block";
    }
    const int depth = (m_outputType == DataType::DT_Float32)
                          ? CV_32F
                          : ((m_outputType == DataType::DT_UInt16) ? CV_16U : CV_8U);
    dst.create(src.rows, src.cols, CV_MAKETYPE(depth, m_targetChannels));
    cv::Mat output = dst.getMat();
    cmsDoTransform(static_cast<cmsHTRANSFORM>(m_transform), src.data, output.data,
                   static_cast<cmsUInt32Number>(src.rows) * static_cast<cmsUInt32Number>(src.cols));
}
