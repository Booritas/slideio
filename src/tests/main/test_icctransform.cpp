#include <gtest/gtest.h>
#include <atomic>
#include <cmath>
#include <thread>
#include <vector>
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

TEST(IccTransform, rejectsByteInputWhenConstructedForUInt16)
{
    // Constructed for DT_UInt16, lcms2's source format is TYPE_RGB_16: it
    // reads 2 bytes/channel. Handing it an 8-bit Mat of the same rows/cols
    // is a heap over-read, not merely wrong output -- this must throw before
    // cmsDoTransform ever runs.
    IccTransform transform(IccTransform::createSRGBProfile(), ColorTarget::Lab,
                           RenderingIntent::RelativeColorimetric, true, DataType::DT_UInt16);
    cv::Mat out;
    ASSERT_THROW(transform.apply(makeRgbPatch({255, 255, 255}), out), slideio::RuntimeError);
}

TEST(IccTransform, rejectsNonContinuousOutput)
{
    // dst.create() is a no-op when the bound array already matches in size and
    // type, so a pre-bound non-continuous ROI reaches cmsDoTransform unchanged
    // and it writes rows*cols*3 contiguous samples into a strided buffer,
    // trampling the row gaps. Symmetric with the input continuity check.
    IccTransform transform(IccTransform::createSRGBProfile(), ColorTarget::sRGB,
                           RenderingIntent::RelativeColorimetric, true, DataType::DT_Byte);
    cv::Mat backing(8, 8, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::Mat roi = backing(cv::Rect(0, 0, 4, 4));
    ASSERT_FALSE(roi.isContinuous());
    ASSERT_THROW(transform.apply(makeRgbPatch({10, 200, 90}), roi), slideio::RuntimeError);
}

TEST(IccTransform, rejectsUInt16InputWhenConstructedForByte)
{
    // The opposite mismatch: constructed for DT_Byte (TYPE_RGB_8, 1
    // byte/channel) but hands it a 16-bit Mat. lcms2 would silently read
    // only the low half of the buffer and produce plausible-looking but
    // wrong colours with no error -- this must throw instead.
    IccTransform transform(IccTransform::createSRGBProfile(), ColorTarget::Lab,
                           RenderingIntent::RelativeColorimetric, true, DataType::DT_Byte);
    cv::Mat wideDepth(4, 4, CV_16UC3, cv::Scalar(60000, 60000, 60000));
    cv::Mat out;
    ASSERT_THROW(transform.apply(wideDepth, out), slideio::RuntimeError);
}

namespace {
    // A deterministic patch, different for every thread index and internally
    // multi-coloured.
    //
    // Both properties are what make the concurrency tests below able to fail.
    // An earlier version had all eight threads transform one constant colour,
    // which cannot observe a shared-state bug at all: whatever a torn
    // per-transform cache paired, the answer was the same colour's answer.
    // Distinct patches make a mispairing visible, and 64 colours per patch
    // stop any one-pixel cache from settling, so shared state is exercised on
    // every pixel rather than once per call.
    cv::Mat makeThreadPatch(int thread)
    {
        cv::Mat patch(8, 8, CV_8UC3);
        for (int row = 0; row < patch.rows; ++row) {
            for (int col = 0; col < patch.cols; ++col) {
                patch.at<cv::Vec3b>(row, col) = cv::Vec3b(
                    static_cast<uchar>((thread * 37 + row * 11 + col * 29) % 256),
                    static_cast<uchar>((thread * 61 + row * 7 + col * 13) % 256),
                    static_cast<uchar>((thread * 97 + row * 5 + col * 3) % 256));
            }
        }
        return patch;
    }

    // Runs one shared transform from eight threads, each on its own patch, and
    // asserts every thread gets exactly the answer that same transform gives
    // that patch single threaded. Exact, not approximate: the reference comes
    // from the same object, so any divergence is concurrency, not tolerance.
    void assertConcurrentApplyMatchesSerial(const IccTransform& transform, int cvType)
    {
        constexpr int threadCount = 8;
        std::vector<cv::Mat> patches;
        std::vector<cv::Mat> expected;
        for (int t = 0; t < threadCount; ++t) {
            patches.push_back(makeThreadPatch(t));
            cv::Mat out;
            transform.apply(patches.back(), out);
            ASSERT_EQ(cvType, out.type());
            expected.push_back(out.clone());
        }

        std::atomic<int> mismatches{0};
        std::vector<std::thread> threads;
        for (int t = 0; t < threadCount; ++t) {
            threads.emplace_back([&transform, &patches, &expected, &mismatches, t]() {
                for (int i = 0; i < 300; ++i) {
                    cv::Mat out;
                    transform.apply(patches[t], out);
                    if (cv::countNonZero(out.reshape(1) != expected[t].reshape(1)) != 0) {
                        ++mismatches;
                    }
                }
            });
        }
        for (auto& thread : threads) {
            thread.join();
        }
        ASSERT_EQ(0, mismatches.load());
    }
}

TEST(IccTransform, applyIsSafeFromSeveralThreads)
{
    // Lab target: mixed formats (TYPE_RGB_8 in, TYPE_Lab_FLT out).
    IccTransform transform(IccTransform::createSRGBProfile(), ColorTarget::Lab,
                           RenderingIntent::RelativeColorimetric, true, DataType::DT_Byte);
    assertConcurrentApplyMatchesSerial(transform, CV_32FC3);
}

TEST(IccTransform, applyIsSafeFromSeveralThreadsOnTheIntegerPath)
{
    // The sRGB target is the one case this class builds where both formatters
    // are integer (TYPE_RGB_8 in and out), which is the only shape lcms2 can
    // route to a *cached* transform -- the path whose one-pixel cache hangs
    // off the shared transform object. Covering it separately keeps the
    // guarantee tested on the path where it is not free, rather than only on
    // the float path that never had a cache.
    IccTransform transform(IccTransform::createSRGBProfile(), ColorTarget::sRGB,
                           RenderingIntent::RelativeColorimetric, true, DataType::DT_Byte);
    assertConcurrentApplyMatchesSerial(transform, CV_8UC3);
}
