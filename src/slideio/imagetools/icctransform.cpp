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

    // 512 bytes is deliberate, not a guess at the maximum: cmsGetProfileInfoASCII
    // truncates to the buffer it is given and never overruns it, and these three
    // fields (description, manufacturer, model) are human-readable labels shown
    // to a user or logged, not identifiers anything matches on. A profile whose
    // description runs longer than 511 characters loses the tail; nothing else
    // in the header depends on it.
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

    // The cv::Mat depth apply() must receive to match the DataType the
    // transform was constructed for. sourceFormat() already rejected every
    // DataType but these two, so this cannot return anything else at apply()
    // time.
    int expectedCvDepth(DataType type)
    {
        return (type == DataType::DT_UInt16) ? CV_16U : CV_8U;
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

    // cmsFLAGS_NOCACHE, unconditionally, for every target.
    //
    // One IccTransform is built at bind time and then shared: every thread
    // reading a colour managed scene calls apply() on the same instance, and
    // the header promises that is safe. lcms2's *cached* transform paths keep
    // a one-pixel cache reachable from the _cmsTRANSFORM object, i.e. state
    // shared between those threads; only this flag makes "immutable after
    // construction" true of the lcms2 object as well as of this wrapper, and
    // so makes the promise depend on nothing but the flag.
    //
    // Set for all four targets rather than for the ones that need it. As of
    // lcms 2.16 a mixed transform (TYPE_RGB_8 in, TYPE_*_FLT out) already
    // takes the uncached float path, so only the sRGB target -- integer in,
    // integer out -- reaches a cached path at all; but that is a fact about
    // one version and about the pixel formats this class happens to
    // construct today. Conditioning the flag on either would leave a trap for
    // whoever adds the next target. The flag is inert on a path that was
    // never cached.
    cmsUInt32Number flags = cmsFLAGS_NOCACHE;
    if (blackPointCompensation) {
        flags |= cmsFLAGS_BLACKPOINTCOMPENSATION;
    }
    m_transform = cmsCreateTransform(srcProfile, srcFormat, dstProfile,
                                     targetFormat(target, sourceType),
                                     toLcmsIntent(intent), flags);
    cmsCloseProfile(srcProfile);
    cmsCloseProfile(dstProfile);
    if (!m_transform) {
        RAISE_RUNTIME_ERROR << "IccTransform: lcms2 could not build a transform to " << target;
    }
    m_outputType = (target == ColorTarget::sRGB) ? sourceType : DataType::DT_Float32;
    m_sourceType = sourceType;
}

void IccTransform::apply(const cv::Mat& src, cv::OutputArray dst) const
{
    if (src.channels() != 3) {
        RAISE_RUNTIME_ERROR << "IccTransform: expected 3 channels, received " << src.channels();
    }
    if (!src.isContinuous()) {
        RAISE_RUNTIME_ERROR << "IccTransform: expected a continuous block";
    }
    const int expectedDepth = expectedCvDepth(m_sourceType);
    if (src.depth() != expectedDepth) {
        // lcms2 reads bytesPerChannel(m_sourceType) * rows * cols * 3 bytes
        // from src.data regardless of what the Mat actually holds. A depth
        // mismatch here is a heap over-read (DT_UInt16 transform, 8-bit
        // input) or a silent half-read producing wrong colours (DT_Byte
        // transform, 16-bit input) -- not a shape/type mismatch OpenCV would
        // catch on its own.
        RAISE_RUNTIME_ERROR << "IccTransform: constructed for " << m_sourceType
                            << " (expected cv::Mat depth " << expectedDepth
                            << "), received a cv::Mat of depth " << src.depth();
    }
    const int depth = (m_outputType == DataType::DT_Float32)
                          ? CV_32F
                          : ((m_outputType == DataType::DT_UInt16) ? CV_16U : CV_8U);
    dst.create(src.rows, src.cols, CV_MAKETYPE(depth, m_targetChannels));
    cv::Mat output = dst.getMat();
    if (!output.isContinuous()) {
        // dst.create() is a no-op when the bound array already has a matching
        // size and type, so a caller that binds a non-continuous ROI gets that
        // ROI back untouched. cmsDoTransform would then write rows*cols*3
        // contiguous samples into a strided buffer, overwriting whatever the
        // row gaps belong to. Symmetric with the input continuity check above,
        // and there for the same reason: apply() is exported and cannot trust
        // its arguments, even though every in-tree caller passes a fresh Mat.
        RAISE_RUNTIME_ERROR << "IccTransform: expected a continuous output block";
    }
    cmsDoTransform(static_cast<cmsHTRANSFORM>(m_transform), src.data, output.data,
                   static_cast<cmsUInt32Number>(src.rows) * static_cast<cmsUInt32Number>(src.cols));
}
