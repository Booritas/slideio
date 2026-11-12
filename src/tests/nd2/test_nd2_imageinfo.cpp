// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <cmath>
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
    // frames 0-5 except 3 exist; T5 x Z2 trims to T3 = 6 frames; frame 3 has no prefix
    // but AcqTimesCache still lists it
    const ImageInfo info = infoOf("frames_missing.nd2");
    EXPECT_EQ(info.frameTimesMs.size(), static_cast<size_t>(info.experiment.loopProduct()));
}

TEST(Nd2ImageInfo, NeTimeStep)
{
    const ImageInfo info = infoOf("ne_time.nd2");
    EXPECT_NEAR(info.tStep, 0.1, 1e-12);
    EXPECT_FALSE(std::isnan(info.tStep));
}

TEST(Nd2ImageInfo, RawJson)
{
    const ImageInfo tpz = infoOf("v3_tpz.nd2");
    const nlohmann::json j = nlohmann::json::parse(tpz.rawJson);
    EXPECT_EQ(j.at("version").get<std::string>(), "3.0");
    EXPECT_TRUE(j.contains("ImageAttributes"));
    EXPECT_TRUE(j.contains("ImageMetadata"));
    EXPECT_TRUE(j.contains("ImageMetadataSeq|0"));
    EXPECT_TRUE(j.contains("ImageTextInfo"));
    EXPECT_EQ(j.at("ImageAttributes").at("uiWidth").get<int>(), 16);

    const nlohmann::json x = nlohmann::json::parse(infoOf("v21_xml.nd2").rawJson);
    EXPECT_EQ(x.at("version").get<std::string>(), "2.1");
    for (const char* key : { "ImageAttributes", "ImageMetadata", "ImageMetadataSeq|0", "ImageTextInfo" }) {
        EXPECT_TRUE(x.contains(key)) << key;
    }
    EXPECT_FALSE(x.contains("ImageCalibration|0"));
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
