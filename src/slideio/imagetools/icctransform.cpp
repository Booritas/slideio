// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/imagetools/icctransform.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/log.hpp"
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

    const cmsUInt32Number version = static_cast<cmsUInt32Number>(cmsGetProfileVersion(handle));
    info.version = std::to_string(version);

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

// The constructor and apply() are implemented in Task 5. Stubs here keep this
// task compiling and reviewable on its own.
IccTransform::IccTransform(const ColorProfile&, ColorTarget, RenderingIntent, bool, DataType)
{
    RAISE_RUNTIME_ERROR << "IccTransform: conversion is not implemented yet";
}

void IccTransform::apply(const cv::Mat&, cv::OutputArray) const
{
    RAISE_RUNTIME_ERROR << "IccTransform: conversion is not implemented yet";
}
