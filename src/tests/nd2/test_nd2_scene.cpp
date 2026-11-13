// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include "slideio/drivers/nd2/nd2imagedriver.hpp"
#include "slideio/drivers/nd2/nd2slide.hpp"
#include "slideio/drivers/nd2/nd2scene.hpp"
#include "slideio/drivers/nd2/nd2framesource.hpp"
#include "slideio/drivers/nd2/nd2chunkfile.hpp"
#include "slideio/drivers/nd2/nd2attributes.hpp"
#include "slideio/drivers/nd2/nd2imageinfo.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/core/tools/filereader.hpp"
#include "slideio/core/tools/tempfile.hpp"
#include "slideio/core/tools/tiledareadownscale.hpp"
#include "tests/testlib/testtools.hpp"
#include "tests/nd2/nd2_fixture_values.hpp"

using namespace slideio;

namespace
{
    std::string fixture(const std::string& name)
    {
        return std::string(ND2_TEST_DATA_DIR) + "/" + name;
    }

    std::shared_ptr<CVSlide> openFixture(const std::string& name)
    {
        ND2ImageDriver driver;
        return driver.openFile(fixture(name));
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

    cv::Mat readPlane(const std::shared_ptr<CVScene>& scene, const cv::Rect& rect, int z, int t,
                      const std::vector<int>& channels = {})
    {
        cv::Mat out;
        scene->readResampledBlockChannelsEx(rect, rect.size(), channels, z, t, out);
        return out;
    }

    // Reads a fixed sequence of rectangles through one cursor over `frame` and checks
    // each against readRows.
    void expectCursorMatches(const std::shared_ptr<const nd2::FrameSource>& frames, int64_t frame,
        int width, int height)
    {
        ASSERT_GE(width, 4);
        ASSERT_GE(height, 4);
        const int half = width / 2;
        const std::vector<cv::Rect> rects = {
            { 0, 0, half, 2 }, { half, 0, width - half, 2 },    // one band, two tiles
            { 0, 2, width, 1 }, { 1, 3, width - 1, 1 },         // the bands below
            { 0, 1, width, 2 },                                 // above: a restart
            { 0, 0, width, height },                            // the whole frame
            { 0, height - 1, width, 1 },                        // the last row again
        };
        auto cursor = frames->openCursor(frame);
        ASSERT_TRUE(cursor);
        for (const cv::Rect& rect : rects) {
            cv::Mat expected, actual;
            frames->readRows(frame, rect, expected);
            cursor->readRows(rect, actual);
            ASSERT_EQ(expected.size(), actual.size()) << rect;
            ASSERT_EQ(expected.type(), actual.type()) << rect;
            EXPECT_EQ(0, cv::norm(expected, actual, cv::NORM_INF)) << rect;
        }
    }

    bool allZero(const cv::Mat& m)
    {
        return cv::countNonZero(m.reshape(1)) == 0;
    }

    // Every pixel of `plane` (rect at `origin`) holds component `channels[c]` of `frame`.
    void expectFrame(const cv::Mat& plane, const cv::Point& origin, const std::vector<int>& components,
                     int64_t frame, DataType type, const std::string& what)
    {
        const cv::Mat d = toDouble(plane);
        ASSERT_EQ(static_cast<int>(components.size()), d.channels()) << what;
        for (int y = 0; y < d.rows; ++y) {
            for (int x = 0; x < d.cols; ++x) {
                for (int c = 0; c < d.channels(); ++c) {
                    const double expected = nd2Expected(origin.x + x, origin.y + y, components[c], frame, 0, type);
                    const double actual = at(d, x, y, c);
                    if (expected != actual) {
                        ADD_FAILURE() << what << ": pixel (" << x << ", " << y << ") channel " << c
                                      << " is " << actual << ", expected " << expected;
                        return;
                    }
                }
            }
        }
    }

    std::vector<int> allComponents(int n)
    {
        std::vector<int> v(n);
        for (int i = 0; i < n; ++i) {
            v[i] = i;
        }
        return v;
    }

    // An open error at the user boundary: "ND2: <cause> (file: <path>)".
    void expectOpenError(const std::string& path, const std::string& text = std::string())
    {
        try {
            ND2Slide::openFile(path);
            ADD_FAILURE() << "expected an exception opening " << path;
        }
        catch (const std::exception& e) {
            const std::string what = e.what();
            EXPECT_EQ(0u, what.rfind("ND2: ", 0)) << what;
            const std::string tail = "(file: " + path + ")";
            EXPECT_TRUE(what.size() >= tail.size() && what.compare(what.size() - tail.size(), tail.size(), tail) == 0)
                << what;
            EXPECT_NE(what.find(text), std::string::npos) << what;
        }
    }
}

TEST(Nd2Slide, OpensBasic)
{
    auto slide = openFixture("v3_basic.nd2");
    ASSERT_EQ(1, slide->getNumScenes());
    EXPECT_EQ(MetadataFormat::JSON, slide->getMetadataFormat());
    EXPECT_FALSE(slide->getRawMetadata().empty());
    auto scene = slide->getScene(0);
    EXPECT_EQ("v3_basic", scene->getName());
    EXPECT_EQ("ND2", scene->getDriverId());
    EXPECT_EQ(cv::Rect(0, 0, 16, 12), scene->getRect());
    ASSERT_EQ(1, scene->getNumChannels());
    EXPECT_EQ("DAPI", scene->getChannelName(0));
    EXPECT_EQ(DataType::DT_Byte, scene->getChannelDataType(0));
    EXPECT_EQ(1, scene->getNumZSlices());
    EXPECT_EQ(1, scene->getNumTFrames());
    const Resolution res = scene->getResolution();
    EXPECT_DOUBLE_EQ(0.5e-6, res.x);
    EXPECT_DOUBLE_EQ(0.5e-6, res.y);
    EXPECT_DOUBLE_EQ(20., scene->getMagnification());
    EXPECT_EQ(Compression::Uncompressed, scene->getCompression());
    EXPECT_EQ(8, scene->getChannelSignificantBits(0));
    EXPECT_EQ(0., scene->getChannelBackground(0));
    EXPECT_EQ(1, scene->getNumZoomLevels());
    const Metadata& attrs = scene->getChannelAttributes();
    ASSERT_EQ(1u, attrs.size());
    EXPECT_EQ("#FF0000", attrs[size_t(0)]["Color"].asString());
    EXPECT_EQ(0, attrs[size_t(0)]["ND2Channel"].asInt());
    EXPECT_EQ(0, attrs[size_t(0)]["Component"].asInt());
}

TEST(Nd2Slide, ScenesArePositions)
{
    auto slide = openFixture("v3_tpz.nd2");
    ASSERT_EQ(2, slide->getNumScenes());
    const char* names[] = {"A", "A (2)"};
    for (int i = 0; i < 2; ++i) {
        auto scene = slide->getScene(i);
        SCOPED_TRACE(names[i]);
        EXPECT_EQ(names[i], scene->getName());
        EXPECT_EQ(i, scene->getSceneIndex());
        EXPECT_EQ(4, scene->getNumZSlices());
        EXPECT_EQ(3, scene->getNumTFrames());
        ASSERT_EQ(2, scene->getNumChannels());
        EXPECT_EQ("DAPI", scene->getChannelName(0));
        EXPECT_EQ("GFP", scene->getChannelName(1));
        EXPECT_EQ(DataType::DT_UInt16, scene->getChannelDataType(1));
        EXPECT_EQ(12, scene->getChannelSignificantBits(0));
        EXPECT_EQ(12, scene->getChannelSignificantBits(1));
        EXPECT_NEAR(1.5e-6, scene->getZSliceResolution(), 1e-15);
        EXPECT_NEAR(0.8, scene->getTFrameResolution(), 1e-12);   // measured, not dPeriod (R28)
        EXPECT_EQ(MetadataFormat::JSON, scene->getMetadataFormat());
        EXPECT_NE(std::string::npos, scene->getRawMetadata().find("dPosName"));
    }
}

TEST(Nd2Slide, Refusals)
{
    for (const char* name : {"bad_width.nd2", "lossy.nd2", "tiled.nd2", "custom_loop.nd2",
                             "two_time_loops.nd2", "bad_comp.nd2", "not_nd2.nd2"}) {
        SCOPED_TRACE(name);
        expectOpenError(fixture(name));
    }
    expectOpenError(fixture("not_nd2.nd2"), "not a Nikon ND2 file");
    expectOpenError(fixture("lossy.nd2"), "eCompression");
}

TEST(Nd2Slide, MissingTrailerRecovered)
{
    TempFile copy("nd2");
    std::filesystem::copy_file(fixture("v3_basic.nd2"), copy.getPath());
    const std::string path = copy.getPath().string();
    {
        // Zero the 40-byte chunk map signature trailer (R18): the chunks are found by scan.
        const auto size = std::filesystem::file_size(path);
        std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
        ASSERT_TRUE(f.good());
        f.seekp(static_cast<std::streamoff>(size - 40));
        const std::string zeros(40, '\0');
        f.write(zeros.data(), zeros.size());
    }
    ND2ImageDriver driver;
    auto damaged = driver.openFile(path);
    auto original = openFixture("v3_basic.nd2");
    ASSERT_EQ(original->getNumScenes(), damaged->getNumScenes());
    for (int s = 0; s < original->getNumScenes(); ++s) {
        SCOPED_TRACE(s);
        auto expected = original->getScene(s);
        auto actual = damaged->getScene(s);
        ASSERT_EQ(expected->getRect(), actual->getRect());
        ASSERT_EQ(expected->getNumChannels(), actual->getNumChannels());
        ASSERT_EQ(expected->getNumZSlices(), actual->getNumZSlices());
        ASSERT_EQ(expected->getNumTFrames(), actual->getNumTFrames());
        const cv::Rect rect(0, 0, expected->getRect().width, expected->getRect().height);
        for (int z = 0; z < expected->getNumZSlices(); ++z) {
            for (int t = 0; t < expected->getNumTFrames(); ++t) {
                const cv::Mat e = readPlane(expected, rect, z, t);
                const cv::Mat a = readPlane(actual, rect, z, t);
                ASSERT_EQ(0, cv::norm(e, a, cv::NORM_INF)) << "z=" << z << " t=" << t;
            }
        }
    }
}

namespace
{
    // The loop order of each fixture, written down from auxfiles/nd2-fixtures (not
    // computed by nd2::Experiment, so that a bug there shows up here).
    struct FixtureOrder
    {
        const char* name;
        int positions;
        int z;
        int t;
        std::function<int64_t(int t, int p, int z)> frame;
    };

    const std::vector<FixtureOrder>& fixtureOrders()
    {
        static const std::vector<FixtureOrder> orders = {
            {"v3_tpz.nd2", 2, 4, 3, [](int t, int p, int z) { return int64_t((t * 2 + p) * 4 + z); }},
            {"order_pz.nd2", 2, 3, 1, [](int, int p, int z) { return int64_t(p * 3 + z); }},
            {"order_zt.nd2", 1, 2, 3, [](int t, int, int z) { return int64_t(z * 3 + t); }},
            {"ne_time.nd2", 1, 2, 2, [](int t, int, int z) { return int64_t(t * 2 + z); }},
            {"v21_xml.nd2", 1, 3, 2, [](int t, int, int z) { return int64_t(t * 3 + z); }},
            {"v20_unpadded.nd2", 1, 3, 1, [](int, int, int z) { return int64_t(z); }},
            {"v20_zlib.nd2", 1, 3, 1, [](int, int, int z) { return int64_t(z); }},
            {"stale_map.nd2", 1, 5, 1, [](int, int, int z) { return int64_t(z); }},
            {"multi_rgb.nd2", 1, 1, 1, [](int, int, int) { return int64_t(0); }},
            {"float32.nd2", 1, 1, 1, [](int, int, int) { return int64_t(0); }},
        };
        return orders;
    }
}

TEST(Nd2Scene, EveryPlane)
{
    for (const FixtureOrder& order : fixtureOrders()) {
        SCOPED_TRACE(order.name);
        auto slide = openFixture(order.name);
        ASSERT_EQ(order.positions, slide->getNumScenes());
        for (int p = 0; p < slide->getNumScenes(); ++p) {
            auto scene = slide->getScene(p);
            ASSERT_EQ(order.z, scene->getNumZSlices());
            ASSERT_EQ(order.t, scene->getNumTFrames());
            const int n = scene->getNumChannels();
            const DataType type = scene->getChannelDataType(0);
            for (int t = 0; t < order.t; ++t) {
                for (int z = 0; z < order.z; ++z) {
                    const cv::Mat plane = readPlane(scene, scene->getRect(), z, t);
                    ASSERT_EQ(scene->getRect().size(), plane.size());
                    const std::string what = std::string(order.name) + " p=" + std::to_string(p)
                        + " z=" + std::to_string(z) + " t=" + std::to_string(t);
                    expectFrame(plane, cv::Point(0, 0), allComponents(n), order.frame(t, p, z), type, what);
                }
            }
        }
    }
}

TEST(Nd2Scene, EveryPlaneChannelCounts)
{
    EXPECT_EQ(3, openFixture("v21_xml.nd2")->getScene(0)->getNumChannels());
    auto rgb = openFixture("multi_rgb.nd2")->getScene(0);
    ASSERT_EQ(12, rgb->getNumChannels());
    EXPECT_EQ("BF1 R", rgb->getChannelName(0));
    EXPECT_EQ("BF4 B", rgb->getChannelName(11));
    const Metadata& attrs = rgb->getChannelAttributes();
    EXPECT_EQ(3, attrs[size_t(11)]["ND2Channel"].asInt());
    EXPECT_EQ(2, attrs[size_t(11)]["Component"].asInt());
    auto f = openFixture("float32.nd2")->getScene(0);
    EXPECT_EQ(DataType::DT_Float32, f->getChannelDataType(0));
    EXPECT_EQ(0, f->getChannelSignificantBits(0));
    EXPECT_EQ(Compression::Zlib, openFixture("v20_zlib.nd2")->getScene(0)->getCompression());
}

TEST(Nd2Scene, MissingFrameIsBackground)
{
    {
        auto scene = openFixture("frames_missing.nd2")->getScene(0);
        ASSERT_EQ(3, scene->getNumTFrames());
        ASSERT_EQ(2, scene->getNumZSlices());
        EXPECT_TRUE(allZero(readPlane(scene, scene->getRect(), 1, 1)));
        expectFrame(readPlane(scene, scene->getRect(), 0, 1), cv::Point(0, 0), {0}, 2, DataType::DT_Byte,
            "frames_missing t=1 z=0");
    }
    {
        auto scene = openFixture("truncated_frame.nd2")->getScene(0);
        ASSERT_EQ(3, scene->getNumZSlices());
        EXPECT_TRUE(allZero(readPlane(scene, scene->getRect(), 2, 0)));
        expectFrame(readPlane(scene, scene->getRect(), 1, 0), cv::Point(0, 0), {0}, 1, DataType::DT_Byte,
            "truncated_frame z=1");
    }
}

TEST(Nd2Scene, ChannelSubsetOrder)
{
    auto scene = openFixture("v21_xml.nd2")->getScene(0);
    const cv::Mat plane = readPlane(scene, scene->getRect(), 1, 1, {2, 0});
    ASSERT_EQ(2, plane.channels());
    expectFrame(plane, cv::Point(0, 0), {2, 0}, 1 * 3 + 1, DataType::DT_Byte, "v21_xml {2, 0}");
}

TEST(Nd2Scene, RightEdgeOfPaddedRow)
{
    auto scene = openFixture("v21_xml.nd2")->getScene(0);
    const cv::Rect rect(12, 0, 3, 5);
    for (int t = 0; t < 2; ++t) {
        for (int z = 0; z < 3; ++z) {
            const cv::Mat plane = readPlane(scene, rect, z, t);
            ASSERT_EQ(rect.size(), plane.size());
            cv::Mat pad;
            cv::compare(plane.reshape(1), cv::Scalar(0xEE), pad, cv::CMP_EQ);
            EXPECT_EQ(0, cv::countNonZero(pad));
            expectFrame(plane, rect.tl(), {0, 1, 2}, t * 3 + z, DataType::DT_Byte, "v21_xml right edge");
        }
    }
}

TEST(Nd2Scene, OutOfRangePlaneThrows)
{
    auto scene = openFixture("v3_tpz.nd2")->getScene(0);
    for (auto zt : {std::make_pair(4, 0), std::make_pair(0, 3), std::make_pair(-1, 0), std::make_pair(0, -1)}) {
        try {
            readPlane(scene, scene->getRect(), zt.first, zt.second);
            ADD_FAILURE() << "z=" << zt.first << " t=" << zt.second << " did not throw";
        }
        catch (const std::exception& e) {
            EXPECT_NE(std::string::npos, std::string(e.what()).find("out of range")) << e.what();
        }
    }
}

TEST(Nd2Scene, Timestamps)
{
    auto slide = openFixture("v3_tpz.nd2");
    auto second = slide->getScene(1);
    ASSERT_EQ("A (2)", second->getName());
    EXPECT_TRUE(second->hasPlaneTimestamps());
    // (2460000.5 - 2440587.5) * 86400 = 1677283200; earliest frame of p=1 is 4 (400 ms).
    EXPECT_EQ(int64_t(1677283200), second->getAcquisitionTime());
    EXPECT_NEAR(0.4, second->getPlaneTimestamp(0, 0, 0), 1e-9);    // frame 4
    EXPECT_NEAR(1.2, second->getPlaneTimestamp(1, 0, 0), 1e-9);    // frame 12
    EXPECT_NEAR(2.3, second->getPlaneTimestamp(2, 1, 3), 1e-9);    // frame 23
    auto first = slide->getScene(0);
    EXPECT_TRUE(first->hasPlaneTimestamps());
    EXPECT_EQ(int64_t(1677283200), first->getAcquisitionTime());
    EXPECT_NEAR(0.8, first->getPlaneTimestamp(1, 0, 0), 1e-9);     // frame 8
    EXPECT_NEAR(0.0, first->getPlaneTimestamp(0, 0, 0), 1e-9);     // frame 0
}

TEST(Nd2Scene, TimestampsWithoutAbsoluteTime)
{
    // Every fixture carries dTimeAbsolute 2460000.5, so this copy of order_pz has it
    // zeroed: in CLX Lite the value follows the UTF-16LE name and its 2-byte terminator.
    std::vector<char> bytes;
    {
        std::ifstream in(fixture("order_pz.nd2"), std::ios::binary);
        bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    const std::string ascii = "dTimeAbsolute";
    std::string name;
    for (char c : ascii) {
        name.push_back(c);
        name.push_back('\0');
    }
    const std::string text(bytes.begin(), bytes.end());
    int patched = 0;
    for (size_t pos = text.find(name); pos != std::string::npos; pos = text.find(name, pos + 1)) {
        const size_t value = pos + name.size() + 2;
        ASSERT_LE(value + sizeof(double), bytes.size());
        double julian = 0;
        std::memcpy(&julian, bytes.data() + value, sizeof(julian));
        ASSERT_EQ(2460000.5, julian);
        std::memset(bytes.data() + value, 0, sizeof(double));
        ++patched;
    }
    ASSERT_GT(patched, 0);
    TempFile copy("nd2");
    {
        std::ofstream out(copy.getPath(), std::ios::binary);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    auto slide = ND2Slide::openFile(copy.getPath().string());
    ASSERT_EQ(2, slide->getNumScenes());
    // Scene p=1 holds frames 3, 4, 5 (300, 400, 500 ms): its earliest frame is not 0,
    // and timestamps are relative to the experiment start (design 6.4).
    auto scene = slide->getScene(1);
    EXPECT_TRUE(scene->hasPlaneTimestamps());
    EXPECT_EQ(0, scene->getAcquisitionTime());
    EXPECT_NEAR(0.3, scene->getPlaneTimestamp(0, 0, 0), 1e-12);
    EXPECT_NEAR(0.5, scene->getPlaneTimestamp(0, 0, 2), 1e-12);
    auto first = slide->getScene(0);
    EXPECT_EQ(0, first->getAcquisitionTime());
    EXPECT_NEAR(0.1, first->getPlaneTimestamp(0, 0, 1), 1e-12);
}

TEST(Nd2Scene, EdgeIsBackground)
{
    auto scene = openFixture("v3_basic.nd2")->getScene(0);
    const cv::Mat out = readPlane(scene, cv::Rect(-4, -4, 8, 8), 0, 0);
    ASSERT_EQ(cv::Size(8, 8), out.size());
    const cv::Mat d = toDouble(out);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            const double expected = (x < 4 || y < 4) ? 0. : nd2Expected(x - 4, y - 4, 0, 0, 0, DataType::DT_Byte);
            ASSERT_EQ(expected, at(d, x, y, 0)) << x << ", " << y;
        }
    }
}

TEST(Nd2Scene, ResampleMatchesCvResize)
{
    auto scene = openFixture("v3_basic.nd2")->getScene(0);
    const cv::Mat full = readPlane(scene, scene->getRect(), 0, 0);
    cv::Mat expected;
    cv::resize(full, expected, cv::Size(8, 6), 0, 0, cv::INTER_AREA);
    cv::Mat out;
    scene->readResampledBlock(scene->getRect(), cv::Size(8, 6), out);
    ASSERT_EQ(expected.size(), out.size());
    ASSERT_EQ(expected.type(), out.type());
    EXPECT_EQ(0, cv::norm(expected, out, cv::NORM_INF));
}

TEST(Nd2Scene, TiledDownscaleThroughCursors)
{
    // A budget of 4 pixels splits every band into column tiles, so the zlib and JPEG 2000
    // cursors serve several tiles per band and several bands per read.
    for (const char* name : { "v20_zlib.nd2", "legacy.nd2", "legacy_rgb.nd2" }) {
        SCOPED_TRACE(name);
        auto banded = ND2Slide::openFile(fixture(name), ResampleLimits{4, std::make_shared<ResampleStats>()});
        auto plain = ND2Slide::openFile(fixture(name));
        for (int i = 0; i < plain->getNumScenes(); ++i) {
            auto a = banded->getScene(i);
            auto b = plain->getScene(i);
            for (int t = 0; t < b->getNumTFrames(); ++t) {
                cv::Mat ra, rb;
                a->readResampledBlockChannelsEx(a->getRect(), cv::Size(2, 2), {}, 0, t, ra);
                b->readResampledBlockChannelsEx(b->getRect(), cv::Size(2, 2), {}, 0, t, rb);
                ASSERT_EQ(rb.size(), ra.size());
                ASSERT_EQ(rb.type(), ra.type());
                EXPECT_EQ(0, cv::norm(ra, rb, cv::NORM_INF)) << a->getName() << " t=" << t;
            }
        }
    }
}

TEST(Nd2Scene, TiledDownscaleWithinBudget)
{
    auto stats = std::make_shared<ResampleStats>();
    auto banded = ND2Slide::openFile(fixture("v3_tpz.nd2"), ResampleLimits{16, stats});
    auto plain = ND2Slide::openFile(fixture("v3_tpz.nd2"));
    ASSERT_EQ(plain->getNumScenes(), banded->getNumScenes());
    for (int i = 0; i < plain->getNumScenes(); ++i) {
        auto a = banded->getScene(i);
        auto b = plain->getScene(i);
        cv::Mat ra, rb;
        a->readResampledBlockChannelsEx(a->getRect(), cv::Size(4, 3), {}, 1, 2, ra);
        b->readResampledBlockChannelsEx(b->getRect(), cv::Size(4, 3), {}, 1, 2, rb);
        ASSERT_EQ(rb.size(), ra.size());
        ASSERT_EQ(rb.type(), ra.type());
        EXPECT_EQ(0, cv::norm(ra, rb, cv::NORM_INF)) << a->getName();
    }
    EXPECT_GT(stats->peakSourcePixels.load(), 0);
    EXPECT_LE(stats->peakSourcePixels.load(), 16);
}

TEST(Nd2Scene, ConcurrentReads)
{
    ND2ImageDriver driver;
    for (const char* name : {"v3_tpz.nd2", "v20_zlib.nd2"}) {
        SCOPED_TRACE(name);
        auto slide = driver.openFile(fixture(name));
        for (int i = 0; i < slide->getNumScenes(); ++i) {
            EXPECT_TRUE(slide->getScene(i)->supportsConcurrentReads());
        }
        TestTools::concurrentReadIdentityTestAllScenes(fixture(name), driver);
    }
}

TEST(Nd2Scene, FileReleasedAfterClose)
{
    TempFile copy("nd2");
    std::filesystem::copy_file(fixture("v3_basic.nd2"), copy.getPath());
    const std::string path = copy.getPath().string();
    {
        ND2ImageDriver driver;
        auto slide = driver.openFile(path);
        auto scene = slide->getScene(0);
        slide.reset();
        cv::Mat out;
        scene->readBlock(scene->getRect(), out);   // the scene outlives its slide
        EXPECT_EQ(nd2Expected(1, 1, 0, 0, 0, DataType::DT_Byte), static_cast<double>(out.at<uint8_t>(1, 1)));
    }
    EXPECT_FALSE(TestTools::isFileHeldOpen(path));
}

TEST(Nd2ImageDriver, ClaimsByNameOnly)
{
    ND2ImageDriver driver;
    EXPECT_EQ("ND2", driver.getID());
    EXPECT_TRUE(driver.canOpenFile("x.ND2"));
    EXPECT_TRUE(driver.canOpenFile("x.nd2"));
    EXPECT_FALSE(driver.canOpenFile("x.nd2x"));
    try {
        driver.openFile(fixture("not_nd2.nd2"));
        ADD_FAILURE() << "not_nd2.nd2 opened";
    }
    catch (const std::exception& e) {
        EXPECT_EQ(0u, std::string(e.what()).rfind("ND2: not a Nikon ND2 file", 0)) << e.what();
    }
}

TEST(Nd2FrameSource, CorruptZlibThrows)
{
    // A copy of v20_zlib with the deflate data of frame 0 overwritten: 0xFF is a final
    // block of the reserved type 3, which inflate rejects.
    const std::string source = fixture("v20_zlib.nd2");
    std::vector<char> bytes;
    {
        std::ifstream in(source, std::ios::binary);
        bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    uint64_t pixels = 0;
    {
        FileReader file(source);
        const nd2::ChunkFile chunks = nd2::ChunkFile::read(file);
        ASSERT_EQ(1u, chunks.frames().count(0));
        pixels = chunks.frames().at(0).dataOffset() + 8;
    }
    ASSERT_LT(pixels + 8, bytes.size());
    std::memset(bytes.data() + pixels + 2, 0xFF, 6);
    TempFile copy("nd2");
    {
        std::ofstream out(copy.getPath(), std::ios::binary);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    auto file = std::make_shared<const FileReader>(copy.getPath().string());
    const nd2::ChunkFile chunks = nd2::ChunkFile::read(*file);
    const nd2::ImageInfo info = nd2::buildImageInfo(chunks, *file, "copy");
    auto frames = nd2::makeChunkFrameSource(file, chunks, info.attributes);
    ASSERT_TRUE(frames->hasFrame(0));
    cv::Mat out;
    try {
        frames->readRows(0, cv::Rect(0, 0, 7, 4), out);
        ADD_FAILURE() << "corrupt stream read";
    }
    catch (const std::exception& e) {
        EXPECT_NE(std::string::npos, std::string(e.what()).find("ND2: frame 0: corrupt zlib stream")) << e.what();
    }
    // The intact frame 1 still reads.
    frames->readRows(1, cv::Rect(0, 0, 7, 4), out);
    expectFrame(out, cv::Point(0, 0), {0}, 1, DataType::DT_UInt16, "v20_zlib frame 1");
}

// PR review: a cursor keeps one decode across the tiles of a read. Tiles along a band,
// the next band down, a band above (which restarts a zlib stream) and the whole frame
// must all read what readRows reads.
TEST(Nd2FrameSource, CursorMatchesReadRows)
{
    auto file = std::make_shared<const FileReader>(fixture("v20_zlib.nd2"));
    const nd2::ChunkFile chunks = nd2::ChunkFile::read(*file);
    const nd2::ImageInfo info = nd2::buildImageInfo(chunks, *file, "v20_zlib");
    ASSERT_EQ(nd2::FrameCompression::Zlib, info.attributes.compression);
    expectCursorMatches(nd2::makeChunkFrameSource(file, chunks, info.attributes), 1,
        info.attributes.width, info.attributes.height);
}
