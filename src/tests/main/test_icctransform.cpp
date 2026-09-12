#include <gtest/gtest.h>
#include "slideio/imagetools/icctransform.hpp"

using namespace slideio;

TEST(IccTransform, describeEmptyProfileReportsAbsent)
{
    const ColorProfileInfo info = IccTransform::describe(ColorProfile());
    ASSERT_FALSE(info.present);
    ASSERT_EQ(ColorProfileSource::None, info.source);
}

TEST(IccTransform, createSRGBProfileIsUsable)
{
    const ColorProfile profile = IccTransform::createSRGBProfile();
    ASSERT_FALSE(profile.isEmpty());
    ASSERT_EQ(ColorProfileSource::Assumed, profile.getSource());
}

TEST(IccTransform, describeSRGBProfileReportsRGBAndXYZ)
{
    const ColorProfile profile = IccTransform::createSRGBProfile();
    const ColorProfileInfo info = IccTransform::describe(profile);
    ASSERT_TRUE(info.present);
    ASSERT_EQ(IccColorSpace::RGB, info.dataSpace);
    ASSERT_EQ(IccColorSpace::XYZ, info.connectionSpace);
    ASSERT_EQ(profile.getSize(), info.dataSize);
    ASSERT_FALSE(info.description.empty());
    // lcms2's cmsCreate_sRGBProfile() builds an ICC v4 profile. Per ICC.1:2010
    // 8.2.18, a v4 profile's mediaWhitePointTag MUST be the PCS illuminant D50
    // (~0.9642, 1.0, 0.8249), not the device's native D65 -- the D65 white is
    // recorded separately, via the chromatic adaptation ("chad") tag, which
    // describe() does not read. Verified against this profile with lcms 2.16:
    // the tag reads (0.964203, 1, 0.824905).
    ASSERT_NEAR(0.9642, info.whitePoint[0], 0.01);
    ASSERT_NEAR(1.0000, info.whitePoint[1], 0.01);
}

TEST(IccTransform, describeSRGBProfileReportsVersion)
{
    // cmsGetProfileVersion() encodes major.minor.bugfix as a single decimal
    // (minor in the tenths place, bugfix in the hundredths place). A
    // previous version of this code truncated that double to an integer
    // before stringifying it, so "4.4" silently became "4". Assert the full
    // string so that regression stays caught.
    const ColorProfile profile = IccTransform::createSRGBProfile();
    const ColorProfileInfo info = IccTransform::describe(profile);
    ASSERT_EQ("4.4", info.version);
}

TEST(IccTransform, describeCarriesSourceFromTheProfile)
{
    // Provenance is known to whoever produced the bytes, not discoverable
    // from the bytes, so describe() must copy it rather than invent it.
    ColorProfile profile = IccTransform::createSRGBProfile();
    profile.setSource(ColorProfileSource::Embedded);
    ASSERT_EQ(ColorProfileSource::Embedded, IccTransform::describe(profile).source);
}

TEST(IccTransform, describeTruncatedProfileReportsAbsentRatherThanThrowing)
{
    // A corrupt profile must not kill a batch read; the caller's policy decides.
    const ColorProfile good = IccTransform::createSRGBProfile();
    std::vector<uint8_t> truncated(good.getData().begin(), good.getData().begin() + 40);
    ColorProfileInfo info;
    ASSERT_NO_THROW(info = IccTransform::describe(ColorProfile(truncated)));
    ASSERT_FALSE(info.present);
}
