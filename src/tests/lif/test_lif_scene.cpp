// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// The slide, scene and read path of the LIF driver, on the synthetic fixtures of
// auxfiles/lif-fixtures/generate.py. Corpus-free.
#include <gtest/gtest.h>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include "slideio/drivers/lif/lifimagedriver.hpp"
#include "slideio/drivers/lif/lifslide.hpp"
#include "slideio/drivers/lif/lifscene.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/core/tools/tempfile.hpp"
#include "tests/testlib/testtools.hpp"
#include "tests/lif/lif_fixture_values.hpp"

using namespace slideio;

namespace
{
    std::string fixture(const std::string& name)
    {
        return std::string(LIF_TEST_DATA_DIR) + "/" + name;
    }

    std::shared_ptr<CVSlide> openFixture(const std::string& name)
    {
        LIFImageDriver driver;
        return driver.openFile(fixture(name));
    }

    std::shared_ptr<CVScene> sceneNamed(const std::shared_ptr<CVSlide>& slide, const std::string& name)
    {
        auto scene = slide->getSceneByName(name);
        if (!scene) {
            ADD_FAILURE() << "no scene named " << name;
        }
        return scene;
    }

    std::vector<std::string> sceneNames(const std::shared_ptr<CVSlide>& slide)
    {
        std::vector<std::string> names;
        for (int i = 0; i < slide->getNumScenes(); ++i) {
            names.push_back(slide->getScene(i)->getName());
        }
        return names;
    }

    cv::Mat toDouble(const cv::Mat& m)
    {
        cv::Mat d;
        m.convertTo(d, CV_MAKETYPE(CV_64F, m.channels()));
        return d;
    }

    double at(const cv::Mat& d, int x, int y, int channel)
    {
        return d.ptr<double>(y)[x * d.channels() + channel];
    }

    cv::Mat readFull(const std::shared_ptr<CVScene>& scene, const std::vector<int>& channels = {})
    {
        cv::Mat out;
        scene->readBlockChannels(scene->getRect(), channels, out);
        return out;
    }

    // An open error at the user boundary starts with "LIF: " and contains `text`.
    void expectOpenError(const std::function<void()>& f, const std::string& text)
    {
        try {
            f();
            ADD_FAILURE() << "expected an exception containing \"" << text << "\"";
        }
        catch (const std::exception& e) {
            const std::string what = e.what();
            EXPECT_EQ(0u, what.rfind("LIF: ", 0)) << what;
            EXPECT_NE(what.find(text), std::string::npos) << what;
        }
    }
}

TEST(LifSlide, OpensBasic)
{
    for (const char* name : {"v2_basic.lif", "v1_basic.lif"}) {
        SCOPED_TRACE(name);
        auto slide = openFixture(name);
        ASSERT_EQ(1, slide->getNumScenes());
        EXPECT_FALSE(slide->getRawMetadata().empty());
        EXPECT_EQ(MetadataFormat::XML, slide->getMetadataFormat());
        auto scene = slide->getScene(0);
        EXPECT_EQ("Plain", scene->getName());
        EXPECT_EQ(cv::Rect(0, 0, 16, 12), scene->getRect());
        EXPECT_EQ(1, scene->getNumChannels());
        EXPECT_EQ(DataType::DT_Byte, scene->getChannelDataType(0));
        EXPECT_EQ(Compression::Uncompressed, scene->getCompression());
        EXPECT_DOUBLE_EQ(63., scene->getMagnification());
        EXPECT_NEAR(1e-6, scene->getResolution().x, 1e-12);
        EXPECT_NEAR(1e-6, scene->getResolution().y, 1e-12);
        EXPECT_FALSE(scene->getRawMetadata().empty());
        EXPECT_EQ(MetadataFormat::XML, scene->getMetadataFormat());
        EXPECT_EQ("LIF", scene->getDriverId());
        EXPECT_EQ(fixture(name), scene->getFilePath());
        EXPECT_EQ(0, scene->getSceneIndex());
        EXPECT_EQ("", scene->getChannelName(0));
        EXPECT_EQ(0., scene->getChannelBackground(0));
        EXPECT_EQ(1, scene->getNumZoomLevels());
    }
}

TEST(LifSlide, ChannelAttributes)
{
    auto slide = openFixture("v2_basic.lif");
    const Metadata& attrs = slide->getScene(0)->getChannelAttributes();
    ASSERT_EQ(1u, attrs.size());
    EXPECT_EQ("Gray", attrs[size_t(0)]["LUTName"].asString());
    EXPECT_EQ("8", attrs[size_t(0)]["Resolution"].asString());
}

TEST(LifSlide, MatchesById)
{
    auto slide = openFixture("order_by_id.lif");
    ASSERT_EQ(2, slide->getNumScenes());
    const cv::Mat a = toDouble(readFull(sceneNamed(slide, "A")));
    const cv::Mat b = toDouble(readFull(sceneNamed(slide, "B")));
    EXPECT_EQ(lifExpected(3, 4, 0, 0, 0, 0, 0, DataType::DT_Byte), at(a, 3, 4, 0));
    EXPECT_EQ(lifExpected(3, 4, 0, 0, 0, 0, 1, DataType::DT_Byte), at(b, 3, 4, 0));
}

TEST(LifSlide, TileFieldsAreScenes)
{
    auto slide = openFixture("nesting.lif");
    ASSERT_EQ(16, slide->getNumScenes());
    const char* orders[] = {"CZMT", "ZCMT", "MCZT", "CZTM"};
    for (int i = 0; i < 16; ++i) {
        auto scene = slide->getScene(i);
        const std::string expected = std::string("Nest/") + orders[i / 4] + "/Field " + std::to_string(i % 4 + 1);
        EXPECT_EQ(expected, scene->getName());
        EXPECT_EQ(i, scene->getSceneIndex());
        EXPECT_EQ(cv::Rect(0, 0, 8, 6), scene->getRect());
        EXPECT_EQ(2, scene->getNumChannels());
        EXPECT_EQ(3, scene->getNumZSlices());
        EXPECT_EQ(2, scene->getNumTFrames());
        EXPECT_EQ(DataType::DT_UInt16, scene->getChannelDataType(1));
        EXPECT_NEAR(4e-6 / 2, scene->getZSliceResolution(), 1e-12);
        EXPECT_NEAR(10., scene->getTFrameResolution(), 1e-9);
    }
    EXPECT_EQ("Nest/CZMT/Field 1", slide->getScene(0)->getName());
    EXPECT_EQ("Nest/CZTM/Field 4", slide->getScene(15)->getName());
}

TEST(LifSlide, SkipsUnsupported)
{
    {
        auto slide = std::dynamic_pointer_cast<LIFSlide>(openFixture("extra_axes.lif"));
        ASSERT_TRUE(slide);
        EXPECT_EQ((std::vector<std::string>{"Extra1", "Plain"}), sceneNames(slide));
        ASSERT_EQ(1u, slide->skippedImages().size());
        EXPECT_EQ(0u, slide->skippedImages()[0].rfind("Lambda: ", 0)) << slide->skippedImages()[0];
    }
    {
        auto slide = std::dynamic_pointer_cast<LIFSlide>(openFixture("broken_refs.lif"));
        EXPECT_EQ((std::vector<std::string>{"Plain"}), sceneNames(slide));
        EXPECT_EQ(3u, slide->skippedImages().size());
        // Plain is the fourth image of the file.
        const cv::Mat d = toDouble(readFull(slide->getScene(0)));
        EXPECT_EQ(lifExpected(5, 6, 0, 0, 0, 0, 3, DataType::DT_Byte), at(d, 5, 6, 0));
    }
    {
        auto slide = std::dynamic_pointer_cast<LIFSlide>(openFixture("bad_strides.lif"));
        EXPECT_EQ((std::vector<std::string>{"Plain"}), sceneNames(slide));
        EXPECT_EQ(2u, slide->skippedImages().size());
    }
    {
        auto slide = std::dynamic_pointer_cast<LIFSlide>(openFixture("truncated.lif"));
        EXPECT_EQ((std::vector<std::string>{"Good"}), sceneNames(slide));
        ASSERT_EQ(1u, slide->skippedImages().size());
        EXPECT_EQ(0u, slide->skippedImages()[0].rfind("Cut: ", 0)) << slide->skippedImages()[0];
    }
    {
        // The empty block before the image is not an image: not reported as skipped.
        auto slide = std::dynamic_pointer_cast<LIFSlide>(openFixture("v2_basic.lif"));
        EXPECT_TRUE(slide->skippedImages().empty());
    }
}

TEST(LifSlide, OpensEveryTopLevelElement)
{
    // An image under each of two top-level elements, with and without a leading
    // XML declaration.
    for (const char* name : {"multi_root.lif", "multi_root_decl.lif"}) {
        SCOPED_TRACE(name);
        auto slide = std::dynamic_pointer_cast<LIFSlide>(openFixture(name));
        ASSERT_TRUE(slide);
        EXPECT_EQ((std::vector<std::string>{"First", "Second"}), sceneNames(slide));
        EXPECT_TRUE(slide->skippedImages().empty());
        ASSERT_EQ(2, slide->getNumScenes());
        const cv::Mat d = toDouble(readFull(slide->getScene(1)));
        EXPECT_EQ(lifExpected(5, 6, 0, 0, 0, 0, 1, DataType::DT_Byte), at(d, 5, 6, 0));
    }
}

TEST(LifSlide, EmptyBlockWithDeclaredSizeIsReported)
{
    // Memory/@Size declares a plane but the block is empty: an image whose data is
    // missing, not a container element, so it is reported rather than dropped.
    auto slide = std::dynamic_pointer_cast<LIFSlide>(openFixture("empty_declared.lif"));
    ASSERT_TRUE(slide);
    EXPECT_EQ((std::vector<std::string>{"Plain"}), sceneNames(slide));
    ASSERT_EQ(1u, slide->skippedImages().size());
    const std::string& reason = slide->skippedImages()[0];
    EXPECT_EQ(0u, reason.rfind("Hollow: ", 0)) << reason;
    EXPECT_NE(std::string::npos, reason.find("Memory/@Size declares 192")) << reason;
}

TEST(LifSlide, MalformedXmlFails)
{
    expectOpenError([] { openFixture("malformed_xml.lif"); }, "LIF: malformed XML description");
}

TEST(LifSlide, AllUnsupportedFails)
{
    expectOpenError([] { openFixture("all_unsupported.lif"); }, "Lambda");
}

TEST(LifSlide, DuplicateNamesKept)
{
    auto slide = openFixture("dup_names.lif");
    EXPECT_EQ((std::vector<std::string>{"Same", "Same"}), sceneNames(slide));
    auto scene = slide->getSceneByName("Same");
    ASSERT_TRUE(scene);
    const cv::Mat d = toDouble(readFull(scene));
    EXPECT_EQ(0., at(d, 0, 0, 0));
}

TEST(LifSlide, UnicodeName)
{
    auto slide = openFixture("unicode.lif");
    ASSERT_EQ(1, slide->getNumScenes());
    // "Zellkern <micro sign> <en dash> <U+7D30 U+80DE>" in UTF-8, spelled as bytes so
    // the source encoding cannot matter.
    EXPECT_EQ(std::string("Zellkern \xC2\xB5 \xE2\x80\x93 \xE7\xB4\xB0\xE8\x83\x9E"), slide->getScene(0)->getName());
}

TEST(LifScene, EveryPlaneOfNesting)
{
    auto slide = openFixture("nesting.lif");
    ASSERT_EQ(16, slide->getNumScenes());
    for (int i = 0; i < slide->getNumScenes(); ++i) {
        auto scene = slide->getScene(i);
        const int salt = i / 4;
        const int field = i % 4;
        for (int z = 0; z < scene->getNumZSlices(); ++z) {
            for (int t = 0; t < scene->getNumTFrames(); ++t) {
                cv::Mat out;
                scene->read4DBlockChannels(scene->getRect(), {}, cv::Range(z, z + 1), cv::Range(t, t + 1), out);
                ASSERT_EQ(CV_16UC2, out.type());
                const cv::Mat d = toDouble(out);
                int mismatches = 0;
                for (int c = 0; c < 2; ++c) {
                    for (int y = 0; y < 6; ++y) {
                        for (int x = 0; x < 8; ++x) {
                            if (at(d, x, y, c) != lifExpected(x, y, c, z, t, field, salt, DataType::DT_UInt16)) {
                                ++mismatches;
                            }
                        }
                    }
                }
                EXPECT_EQ(0, mismatches) << scene->getName() << " z=" << z << " t=" << t;
            }
        }
    }
}

TEST(LifScene, TypesRoundTrip)
{
    struct Expect { const char* name; DataType type; int salt; };
    const Expect cases[] = {
        {"U8", DataType::DT_Byte, 0},
        {"U16_12bit", DataType::DT_UInt16, 1},
        {"F16", DataType::DT_Float16, 3},
        {"F32", DataType::DT_Float32, 4},
        {"F64", DataType::DT_Float64, 5},
        {"NoDataType16", DataType::DT_UInt16, 6},
    };
    auto slide = openFixture("types.lif");
    // Mismatch (stride against type) and U32 (no OpenCV depth) are skipped.
    EXPECT_EQ(6, slide->getNumScenes());
    const auto& skipped = std::dynamic_pointer_cast<LIFSlide>(slide)->skippedImages();
    ASSERT_EQ(2u, skipped.size());
    EXPECT_EQ(0u, skipped[0].rfind("U32: ", 0)) << skipped[0];
    EXPECT_NE(std::string::npos, skipped[0].find("32-bit unsigned")) << skipped[0];
    EXPECT_EQ(0u, skipped[1].rfind("Mismatch: ", 0)) << skipped[1];
    EXPECT_FALSE(slide->getSceneByName("U32"));
    for (const auto& e : cases) {
        SCOPED_TRACE(e.name);
        auto scene = sceneNamed(slide, e.name);
        ASSERT_TRUE(scene);
        EXPECT_EQ(e.type, scene->getChannelDataType(0));
        const cv::Mat d = toDouble(readFull(scene));
        EXPECT_EQ(lifExpected(5, 7, 0, 0, 0, 0, e.salt, e.type), at(d, 5, 7, 0));
    }
    EXPECT_EQ(12, sceneNamed(slide, "U16_12bit")->getChannelSignificantBits(0));
    EXPECT_EQ(8, sceneNamed(slide, "U8")->getChannelSignificantBits(0));
}

TEST(LifScene, RgbOrder)
{
    auto slide = openFixture("rgb.lif");
    struct Expect { const char* name; int salt; int redSample; };
    const Expect cases[] = {{"TagRGB", 0, 0}, {"TagBGR", 1, 2}, {"Untagged", 2, 2}};
    for (const auto& e : cases) {
        SCOPED_TRACE(e.name);
        auto scene = sceneNamed(slide, e.name);
        ASSERT_TRUE(scene);
        EXPECT_EQ(3, scene->getNumChannels());
        const cv::Mat d = toDouble(readFull(scene));
        ASSERT_EQ(3, d.channels());
        for (int y = 0; y < 6; ++y) {
            for (int x = 0; x < 8; ++x) {
                ASSERT_EQ(lifExpected(x, y, e.redSample, 0, 0, 0, e.salt, DataType::DT_Byte), at(d, x, y, 0))
                    << x << "," << y;
                ASSERT_EQ(lifExpected(x, y, 1, 0, 0, 0, e.salt, DataType::DT_Byte), at(d, x, y, 1));
                ASSERT_EQ(lifExpected(x, y, 2 - e.redSample, 0, 0, 0, e.salt, DataType::DT_Byte), at(d, x, y, 2));
            }
        }
    }
}

TEST(LifScene, ChannelSubsetOrder)
{
    auto slide = openFixture("rgb.lif");
    auto scene = sceneNamed(slide, "TagRGB");
    ASSERT_TRUE(scene);
    const cv::Mat d = toDouble(readFull(scene, {2, 0}));
    ASSERT_EQ(2, d.channels());
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 8; ++x) {
            ASSERT_EQ(lifExpected(x, y, 2, 0, 0, 0, 0, DataType::DT_Byte), at(d, x, y, 0));
            ASSERT_EQ(lifExpected(x, y, 0, 0, 0, 0, 0, DataType::DT_Byte), at(d, x, y, 1));
        }
    }
}

TEST(LifScene, XzScan)
{
    auto slide = openFixture("xzxt.lif");
    auto xz = sceneNamed(slide, "XZ");
    ASSERT_TRUE(xz);
    EXPECT_EQ(cv::Rect(0, 0, 16, 5), xz->getRect());
    EXPECT_EQ(1, xz->getNumZSlices());
    // Z is the display Y here: its spacing is the Y resolution, not a slice spacing.
    EXPECT_EQ(0., xz->getZSliceResolution());
    EXPECT_NEAR(5e-6 / 4, xz->getResolution().y, 1e-12);
    const cv::Mat d = toDouble(readFull(xz));
    EXPECT_EQ(lifExpected(2, 0, 0, 3, 0, 0, 0, DataType::DT_Byte), at(d, 2, 3, 0));
    auto xt = sceneNamed(slide, "XT");
    ASSERT_TRUE(xt);
    EXPECT_EQ(cv::Rect(0, 0, 16, 7), xt->getRect());
    EXPECT_EQ(1, xt->getNumTFrames());
    const cv::Mat dt = toDouble(readFull(xt));
    EXPECT_EQ(lifExpected(4, 0, 0, 0, 6, 0, 1, DataType::DT_Byte), at(dt, 4, 6, 0));
}

TEST(LifScene, PaddedRows)
{
    auto slide = openFixture("padded.lif");
    auto scene = slide->getScene(0);
    const cv::Mat d = toDouble(readFull(scene));
    for (int y = 0; y < 12; ++y) {
        for (int x = 0; x < 16; ++x) {
            ASSERT_EQ(lifExpected(x, y, 0, 0, 0, 0, 0, DataType::DT_Byte), at(d, x, y, 0)) << x << "," << y;
        }
    }
    // A sub-rectangle goes through the per-row path as well.
    cv::Mat part;
    scene->readBlock(cv::Rect(3, 2, 5, 4), part);
    const cv::Mat dp = toDouble(part);
    EXPECT_EQ(lifExpected(3 + 4, 2 + 3, 0, 0, 0, 0, 0, DataType::DT_Byte), at(dp, 4, 3, 0));
}

TEST(LifScene, Timestamps)
{
    auto slide = openFixture("timestamps.lif");
    auto hex = sceneNamed(slide, "Hex");
    ASSERT_TRUE(hex);
    EXPECT_TRUE(hex->hasPlaneTimestamps());
    EXPECT_EQ(1455526400, hex->getAcquisitionTime());
    // storage index k = c + 2*t, one second apart
    EXPECT_DOUBLE_EQ(3., hex->getPlaneTimestamp(1, 1, 0));
    auto dec = sceneNamed(slide, "Decimal");
    ASSERT_TRUE(dec);
    EXPECT_TRUE(dec->hasPlaneTimestamps());
    auto shortList = sceneNamed(slide, "Short");
    ASSERT_TRUE(shortList);
    EXPECT_FALSE(shortList->hasPlaneTimestamps());
    EXPECT_EQ(0, shortList->getAcquisitionTime());
}

TEST(LifScene, EdgeIsBackground)
{
    auto slide = openFixture("v2_basic.lif");
    auto scene = slide->getScene(0);
    cv::Mat out;
    scene->readBlock(cv::Rect(-4, -4, 8, 8), out);
    ASSERT_EQ(cv::Size(8, 8), out.size());
    const cv::Mat d = toDouble(out);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            const double expected = (x < 4 || y < 4)
                ? 0. : lifExpected(x - 4, y - 4, 0, 0, 0, 0, 0, DataType::DT_Byte);
            ASSERT_EQ(expected, at(d, x, y, 0)) << x << "," << y;
        }
    }
}

TEST(LifScene, ResampleMatchesCvResize)
{
    auto slide = openFixture("v2_basic.lif");
    auto scene = slide->getScene(0);
    const cv::Mat full = readFull(scene);
    cv::Mat expected;
    cv::resize(full, expected, cv::Size(8, 6), 0, 0, cv::INTER_AREA);
    cv::Mat out;
    scene->readResampledBlock(scene->getRect(), cv::Size(8, 6), out);
    ASSERT_EQ(expected.size(), out.size());
    ASSERT_EQ(expected.type(), out.type());
    EXPECT_EQ(0, cv::norm(expected, out, cv::NORM_INF));
}

TEST(LifScene, BandedResampleEqualsUnbandedAtIntegerRatio)
{
    lif::ReadLimits tiny;
    tiny.maxResampleSourcePixels = 16;
    auto banded = LIFSlide::openFile(fixture("nesting.lif"), tiny);
    auto plain = LIFSlide::openFile(fixture("nesting.lif"));
    ASSERT_EQ(plain->getNumScenes(), banded->getNumScenes());
    for (int i = 0; i < plain->getNumScenes(); ++i) {
        auto a = banded->getScene(i);
        auto b = plain->getScene(i);
        cv::Mat ra, rb;
        a->readResampled4DBlockChannels(a->getRect(), cv::Size(4, 3), {}, cv::Range(1, 2), cv::Range(1, 2), ra);
        b->readResampled4DBlockChannels(b->getRect(), cv::Size(4, 3), {}, cv::Range(1, 2), cv::Range(1, 2), rb);
        ASSERT_EQ(rb.size(), ra.size());
        ASSERT_EQ(rb.type(), ra.type());
        EXPECT_EQ(0, cv::norm(ra, rb, cv::NORM_INF)) << a->getName();
    }
}

TEST(LifScene, UpscaleIsNeverBanded)
{
    // An upscale over the budget is read whole: bands would leave seams in an
    // interpolating upscale and save no memory.
    lif::ReadLimits tiny;
    tiny.maxResampleSourcePixels = 16;
    auto a = LIFSlide::openFile(fixture("v2_basic.lif"), tiny)->getScene(0);
    auto b = LIFSlide::openFile(fixture("v2_basic.lif"))->getScene(0);
    for (int height = 13; height <= 40; ++height) {
        cv::Mat ra, rb;
        a->readResampledBlock(a->getRect(), cv::Size(16, height), ra);
        b->readResampledBlock(b->getRect(), cv::Size(16, height), rb);
        ASSERT_EQ(rb.size(), ra.size());
        EXPECT_EQ(0, cv::norm(ra, rb, cv::NORM_INF)) << "height " << height;
    }
}

TEST(LifScene, ConcurrentReads)
{
    LIFImageDriver driver;
    auto slide = driver.openFile(fixture("nesting.lif"));
    for (int i = 0; i < slide->getNumScenes(); ++i) {
        EXPECT_TRUE(slide->getScene(i)->supportsConcurrentReads());
    }
    TestTools::concurrentReadIdentityTestAllScenes(fixture("nesting.lif"), driver);
}

TEST(LifScene, FileReleasedAfterClose)
{
    TempFile copy("lif");
    std::filesystem::copy_file(fixture("v2_basic.lif"), copy.getPath());
    const std::string path = copy.getPath().string();
    {
        LIFImageDriver driver;
        auto slide = driver.openFile(path);
        auto scene = slide->getScene(0);
        slide.reset();
        cv::Mat out;
        scene->readBlock(scene->getRect(), out);   // the scene outlives its slide
        EXPECT_EQ(lifExpected(1, 1, 0, 0, 0, 0, 0, DataType::DT_Byte), static_cast<double>(out.at<uint8_t>(1, 1)));
    }
    EXPECT_FALSE(TestTools::isFileHeldOpen(path));
}

TEST(LifImageDriver, ClaimsByNameOnly)
{
    LIFImageDriver driver;
    EXPECT_EQ("LIF", driver.getID());
    EXPECT_EQ("*.lif", driver.getFileSpecs());
    EXPECT_TRUE(driver.canOpenFile("x.LIF"));
    EXPECT_TRUE(driver.canOpenFile("x.lif"));
    EXPECT_FALSE(driver.canOpenFile("x.lifext"));
    expectOpenError([&] { driver.openFile(fixture("not_lif.lif")); }, "LIF: not a Leica LIF file");
    expectOpenError([&] { driver.openFile(fixture("not_lif.lif")); }, "(file: ");
}

TEST(LifSlide, OpenErrorsStartWithLif)
{
    // The file:line prefix of the internal error is dropped...
    try {
        openFixture("lof.lif");
        ADD_FAILURE() << "lof.lif opened";
    }
    catch (const std::exception& e) {
        const std::string what = e.what();
        EXPECT_EQ(0u, what.rfind("LIF: ", 0)) << what;
        EXPECT_EQ(std::string::npos, what.find(".cpp:")) << what;
    }
    // ...and an error from outside the driver gains the prefix.
    expectOpenError([] { openFixture("does_not_exist.lif"); }, "does_not_exist.lif");
}
