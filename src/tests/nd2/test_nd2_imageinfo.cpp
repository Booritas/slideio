// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include "slideio/drivers/nd2/nd2imageinfo.hpp"
#include "slideio/core/tools/filereader.hpp"

using namespace slideio;
using namespace slideio::nd2;

namespace
{
    ImageInfo infoOf(const char* name)
    {
        const std::string path = std::string(ND2_TEST_DATA_DIR) + "/" + name;
        FileReader file(path);
        const ChunkFile chunks = ChunkFile::read(file);
        std::string stem = name;
        stem = stem.substr(0, stem.rfind('.'));
        return buildImageInfo(chunks, file, stem);
    }
}

TEST(Nd2ImageInfo, Calibration)
{
    const ImageInfo basic = infoOf("v3_basic.nd2");
    EXPECT_NEAR(basic.pixelSizeX, 0.5e-6, 1e-15);
    EXPECT_NEAR(basic.pixelSizeY, 0.5e-6, 1e-15);
    EXPECT_EQ(basic.magnification, 20);
    EXPECT_EQ(basic.absoluteJulianDay, 2460000.5);
    EXPECT_EQ(basic.zStep, 0);
    const ImageInfo tpz = infoOf("v3_tpz.nd2");
    EXPECT_NEAR(tpz.zStep, 1.5e-6, 1e-15);
    EXPECT_DOUBLE_EQ(tpz.tStep, 2.0);
    EXPECT_EQ(tpz.attributes.width, 16);
    EXPECT_EQ(tpz.channels.size(), 2u);
}

TEST(Nd2ImageInfo, Positions)
{
    const ImageInfo tpz = infoOf("v3_tpz.nd2");
    ASSERT_EQ(tpz.positions.size(), 2u);
    EXPECT_EQ(tpz.positions[0].name, "A");
    EXPECT_EQ(tpz.positions[1].name, "A (2)");
    // the point records are Points.i0 and Points.i2 (the second point is invalid)
    EXPECT_EQ(nlohmann::json::parse(tpz.positions[1].rawJson).at("dPosX").get<double>(), 2000.0);

    const ImageInfo pz = infoOf("order_pz.nd2");
    ASSERT_EQ(pz.positions.size(), 2u);
    EXPECT_EQ(pz.positions[0].name, "Position 1");
    EXPECT_EQ(pz.positions[1].name, "Position 2");

    const ImageInfo basic = infoOf("v3_basic.nd2");
    ASSERT_EQ(basic.positions.size(), 1u);
    EXPECT_EQ(basic.positions[0].name, "v3_basic");
    EXPECT_TRUE(basic.positions[0].rawJson.empty());
}

TEST(Nd2ImageInfo, FrameTimes)
{
    const ImageInfo tpz = infoOf("v3_tpz.nd2");
    ASSERT_EQ(tpz.frameTimesMs.size(), 24u);
    EXPECT_EQ(tpz.frameTimesMs[5], 500.0);
    // v20_unpadded has no AcqTimesCache: the times come from the frame prefixes
    const ImageInfo v20 = infoOf("v20_unpadded.nd2");
    ASSERT_EQ(v20.frameTimesMs.size(), 3u);
    EXPECT_EQ(v20.frameTimesMs[0], 0.0);
    EXPECT_EQ(v20.frameTimesMs[2], 200.0);
}

TEST(Nd2ImageInfo, FrameTimesMissingFrames)
{
    // frames 0-5 except 3 exist; T5 x Z2 trims to T3 = 6 frames. The cache covers frame 3,
    // so its time is the cache value although the frame itself is absent.
    const ImageInfo info = infoOf("frames_missing.nd2");
    ASSERT_EQ(info.frameTimesMs.size(), 6u);
    EXPECT_EQ(info.frameTimesMs[3], 300.0);
    EXPECT_EQ(info.frameTimesMs[4], 400.0);
    EXPECT_EQ(info.frameTimesMs[0], 0.0);
}

TEST(Nd2ImageInfo, MergeFrameTimesShortCache)
{
    // The cache covers 2 of 5 frames; frames 2 and 4 exist, frame 3 does not.
    const std::vector<double> cache = { 0.0, 100.0 };
    const std::vector<double> t = mergeFrameTimes(cache, 5, [](int64_t frame, double& out) {
        if (frame == 2 || frame == 4) {
            out = 1000.0 + static_cast<double>(frame);
            return true;
        }
        return false;
    });
    ASSERT_EQ(t.size(), 5u);
    EXPECT_EQ(t[0], 0.0);
    EXPECT_EQ(t[1], 100.0);
    EXPECT_EQ(t[2], 1002.0);
    EXPECT_TRUE(std::isnan(t[3]));
    EXPECT_EQ(t[4], 1004.0);
    // a cache longer than the loop product is clipped and the reader is not consulted
    const std::vector<double> clipped = mergeFrameTimes({ 1, 2, 3, 4 }, 2, [](int64_t, double&) {
        ADD_FAILURE() << "prefix reader must not run";
        return false;
    });
    EXPECT_EQ(clipped, (std::vector<double>{ 1, 2 }));
    // no cache at all: everything comes from the prefixes
    const std::vector<double> none = mergeFrameTimes({}, 2, [](int64_t f, double& out) {
        out = 10.0 * static_cast<double>(f);
        return true;
    });
    EXPECT_EQ(none, (std::vector<double>{ 0, 10 }));
}

namespace
{
    Experiment timeLoop(int count)
    {
        const std::string xml =
            "<variant><no_name runtype=\"CLxListVariant\"><eType runtype=\"lx_uint32\" value=\"1\"/>"
            "<uLoopPars runtype=\"CLxListVariant\"><uiCount runtype=\"lx_uint32\" value=\"" +
            std::to_string(count) + "\"/></uLoopPars></no_name></variant>";
        const Variant v = decodeMetadata(std::vector<uint8_t>(xml.begin(), xml.end()));
        return Experiment::parse(&v);
    }
}

TEST(Nd2ImageInfo, MeanStepFallback)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const Experiment three = timeLoop(3);
    ASSERT_EQ(three.count(Axis::T), 3);
    EXPECT_NEAR(meanStepSeconds(three, { 0.0, 100.0, 200.0 }), 0.1, 1e-12);
    EXPECT_NEAR(meanStepSeconds(three, { 50.0, 250.0, 450.0 }), 0.2, 1e-12);
    EXPECT_EQ(meanStepSeconds(three, { 0.0, nan, 200.0 }), 0.0);
    EXPECT_EQ(meanStepSeconds(three, { nan, 100.0, 200.0 }), 0.0);
    EXPECT_EQ(meanStepSeconds(three, { 0.0, 100.0 }), 0.0);  // too few times
    const Experiment one = timeLoop(1);
    EXPECT_EQ(meanStepSeconds(one, { 0.0 }), 0.0);
}

TEST(Nd2ImageInfo, NeTimeStep)
{
    // ne_time: periods 100 ms and 100 ms, the first valid one is used
    const ImageInfo info = infoOf("ne_time.nd2");
    EXPECT_NEAR(info.tStep, 0.1, 1e-12);
}

namespace
{
    std::set<std::string> keysOf(const nlohmann::json& j)
    {
        std::set<std::string> keys;
        for (auto it = j.begin(); it != j.end(); ++it) {
            keys.insert(it.key());
        }
        return keys;
    }
}

TEST(Nd2ImageInfo, RawJson)
{
    const nlohmann::json j = nlohmann::json::parse(infoOf("v3_tpz.nd2").rawJson);
    const nlohmann::json x = nlohmann::json::parse(infoOf("v21_xml.nd2").rawJson);
    // Neither fixture has a calibration chunk; both have text info.
    const std::set<std::string> expected = { "version", "ImageAttributes", "ImageMetadata",
        "ImageMetadataSeq|0", "ImageTextInfo" };
    EXPECT_EQ(keysOf(j), expected);
    EXPECT_EQ(keysOf(x), expected);
    EXPECT_EQ(keysOf(j), keysOf(x));
    EXPECT_EQ(j.at("version").get<std::string>(), "3.0");
    EXPECT_EQ(x.at("version").get<std::string>(), "2.1");
    EXPECT_EQ(j.at("ImageAttributes").at("uiWidth").get<int>(), 16);
}

TEST(Nd2ImageInfo, ZStepFallback)
{
    const std::string xml =
        "<variant><no_name runtype=\"CLxListVariant\"><uiCount runtype=\"lx_uint32\" value=\"4\"/>"
        "<dZStep runtype=\"double\" value=\"0\"/><dZLow runtype=\"double\" value=\"0\"/>"
        "<dZHigh runtype=\"double\" value=\"6\"/></no_name></variant>";
    const Variant v = decodeMetadata(std::vector<uint8_t>(xml.begin(), xml.end()));
    EXPECT_NEAR(zStepFrom(v), 2e-6, 1e-15);
}
