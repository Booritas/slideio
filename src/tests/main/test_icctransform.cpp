#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include "slideio/imagetools/icctransform.hpp"
#include "slideio/core/exceptions.hpp"

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

namespace {
    cv::Mat makeRgbPatch(const cv::Vec3b& colour)
    {
        cv::Mat patch(4, 4, CV_8UC3);
        patch.setTo(cv::Scalar(colour[0], colour[1], colour[2]));
        return patch;
    }
}

TEST(IccTransform, srgbToLabWhiteIsLightness100)
{
    IccTransform transform(IccTransform::createSRGBProfile(), ColorTarget::Lab,
                           RenderingIntent::RelativeColorimetric, true, DataType::DT_Byte);
    ASSERT_EQ(DataType::DT_Float32, transform.getOutputDataType());
    cv::Mat out;
    transform.apply(makeRgbPatch({255, 255, 255}), out);
    ASSERT_EQ(CV_32FC3, out.type());
    const cv::Vec3f lab = out.at<cv::Vec3f>(0, 0);
    ASSERT_NEAR(100.0f, lab[0], 0.5f);
    ASSERT_NEAR(0.0f, lab[1], 1.0f);
    ASSERT_NEAR(0.0f, lab[2], 1.0f);
}

TEST(IccTransform, srgbToLabMidGreyIsLightness53)
{
    IccTransform transform(IccTransform::createSRGBProfile(), ColorTarget::Lab,
                           RenderingIntent::RelativeColorimetric, true, DataType::DT_Byte);
    cv::Mat out;
    transform.apply(makeRgbPatch({128, 128, 128}), out);
    ASSERT_NEAR(53.6f, out.at<cv::Vec3f>(0, 0)[0], 1.0f);
}

TEST(IccTransform, channelOrderIsRgbNotBgr)
{
    // The regression that matters: a BGR mix-up produces output of the right
    // shape and dtype that looks entirely plausible. Pure red must stay red,
    // which in Lab means a strongly positive a* and a near-zero-to-positive b*.
    IccTransform transform(IccTransform::createSRGBProfile(), ColorTarget::Lab,
                           RenderingIntent::RelativeColorimetric, true, DataType::DT_Byte);
    cv::Mat out;
    transform.apply(makeRgbPatch({255, 0, 0}), out);
    const cv::Vec3f lab = out.at<cv::Vec3f>(0, 0);
    ASSERT_NEAR(53.2f, lab[0], 1.5f);   // sRGB red
    ASSERT_GT(lab[1], 60.0f);           // a* strongly positive
    ASSERT_GT(lab[2], 40.0f);           // b* positive; blue would give a large negative
}

TEST(IccTransform, srgbToSrgbPreservesDataType)
{
    IccTransform transform(IccTransform::createSRGBProfile(), ColorTarget::sRGB,
                           RenderingIntent::RelativeColorimetric, true, DataType::DT_Byte);
    ASSERT_EQ(DataType::DT_Byte, transform.getOutputDataType());
    cv::Mat out;
    transform.apply(makeRgbPatch({10, 200, 90}), out);
    ASSERT_EQ(CV_8UC3, out.type());
    const cv::Vec3b rgb = out.at<cv::Vec3b>(0, 0);
    ASSERT_NEAR(10, rgb[0], 2);
    ASSERT_NEAR(200, rgb[1], 2);
    ASSERT_NEAR(90, rgb[2], 2);
}

TEST(IccTransform, linearRgbRemovesGamma)
{
    IccTransform transform(IccTransform::createSRGBProfile(), ColorTarget::LinearRGB,
                           RenderingIntent::RelativeColorimetric, true, DataType::DT_Byte);
    ASSERT_EQ(DataType::DT_Float32, transform.getOutputDataType());
    cv::Mat out;
    transform.apply(makeRgbPatch({128, 128, 128}), out);
    // sRGB 128/255 is about 0.216 once linearised, not 0.502.
    ASSERT_NEAR(0.216f, out.at<cv::Vec3f>(0, 0)[0], 0.02f);
}

TEST(IccTransform, xyzTargetProducesFloat)
{
    IccTransform transform(IccTransform::createSRGBProfile(), ColorTarget::XYZ,
                           RenderingIntent::RelativeColorimetric, true, DataType::DT_Byte);
    ASSERT_EQ(DataType::DT_Float32, transform.getOutputDataType());
    cv::Mat out;
    transform.apply(makeRgbPatch({255, 255, 255}), out);
    ASSERT_NEAR(1.0f, out.at<cv::Vec3f>(0, 0)[1], 0.02f);   // Y of white
}

TEST(IccTransform, rejectsNonThreeChannelInput)
{
    IccTransform transform(IccTransform::createSRGBProfile(), ColorTarget::Lab,
                           RenderingIntent::RelativeColorimetric, true, DataType::DT_Byte);
    cv::Mat grey(4, 4, CV_8UC1, cv::Scalar(128));
    cv::Mat out;
    ASSERT_THROW(transform.apply(grey, out), slideio::RuntimeError);
}

TEST(IccTransform, applyIsSafeFromSeveralThreads)
{
    IccTransform transform(IccTransform::createSRGBProfile(), ColorTarget::Lab,
                           RenderingIntent::RelativeColorimetric, true, DataType::DT_Byte);
    std::vector<std::thread> threads;
    std::atomic<int> failures{0};
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&transform, &failures]() {
            for (int i = 0; i < 200; ++i) {
                cv::Mat out;
                transform.apply(makeRgbPatch({255, 255, 255}), out);
                if (std::abs(out.at<cv::Vec3f>(0, 0)[0] - 100.0f) > 0.5f) {
                    ++failures;
                }
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    ASSERT_EQ(0, failures.load());
}
