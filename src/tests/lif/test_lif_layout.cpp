// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "slideio/drivers/lif/liflayout.hpp"
#include "slideio/drivers/lif/lifxml.hpp"
#include "slideio/drivers/lif/lifcontainer.hpp"
#include "slideio/core/tools/filereader.hpp"

using namespace slideio;
using namespace slideio::lif;

namespace
{
    struct FixtureImage
    {
        ImageDescription description;
        uint64_t payloadLength = 0;
    };

    std::vector<FixtureImage> loadFixture(const char* name)
    {
        FileReader file(std::string(LIF_TEST_DATA_DIR) + "/" + name);
        const Container c = Container::read(file);
        std::vector<FixtureImage> out;
        for (auto& d : parseImages(c.description())) {
            const MemoryBlock* block = c.findBlock(d.memoryId);
            FixtureImage fi;
            fi.payloadLength = block ? block->length : 0;
            fi.description = std::move(d);
            out.push_back(std::move(fi));
        }
        return out;
    }

    const FixtureImage& named(const std::vector<FixtureImage>& v, const std::string& path)
    {
        for (const auto& f : v) {
            if (f.description.path == path) {
                return f;
            }
        }
        throw std::runtime_error("no image " + path);
    }

    Layout buildNamed(const std::vector<FixtureImage>& v, const std::string& path)
    {
        const FixtureImage& f = named(v, path);
        return Layout::build(f.description, f.payloadLength);
    }

    Dimension dim(int id, int64_t count, int64_t inc, double length = 0, const std::string& unit = "m")
    {
        Dimension d;
        d.dimId = id;
        d.count = count;
        d.bytesInc = inc;
        d.length = length;
        d.unit = unit;
        return d;
    }

    Channel channel(int64_t inc, int dataType = 0, int resolution = 8)
    {
        Channel c;
        c.dataType = dataType;
        c.resolution = resolution;
        c.bytesInc = inc;
        return c;
    }

    // W x H uint8, one channel, X/Y lengths in metres.
    ImageDescription plainImage(int64_t w, int64_t h)
    {
        ImageDescription d;
        d.dimensions = {dim(1, w, 1, 1.5e-5), dim(2, h, w, 1.1e-5)};
        d.channels = {channel(0)};
        return d;
    }

    // Unsupported whose message contains the given text.
    template <typename F>
    void expectUnsupported(F f, const std::string& text)
    {
        try {
            f();
            ADD_FAILURE() << "expected Unsupported containing \"" << text << "\"";
        }
        catch (const Unsupported& e) {
            EXPECT_NE(std::string::npos, std::string(e.what()).find(text)) << e.what();
        }
    }
}

TEST(LifLayout, NestingOffsets)
{
    const auto v = loadFixture("nesting.lif");
    struct Strides { const char* name; uint64_t c, z, m, t; };
    const Strides expected[] = {
        {"Nest/CZMT", 96, 192, 576, 2304},
        {"Nest/ZCMT", 288, 96, 576, 2304},
        {"Nest/MCZT", 384, 768, 96, 2304},
        {"Nest/CZTM", 96, 192, 1152, 576},
    };
    for (const auto& s : expected) {
        SCOPED_TRACE(s.name);
        const Layout l = buildNamed(v, s.name);
        EXPECT_EQ(8, l.width());
        EXPECT_EQ(6, l.height());
        EXPECT_EQ(2, l.numChannels());
        EXPECT_EQ(3, l.numZ());
        EXPECT_EQ(2, l.numT());
        EXPECT_EQ(4, l.numFields());
        EXPECT_EQ(DataType::DT_UInt16, l.dataType());
        EXPECT_EQ(2, l.sampleBytes());
        EXPECT_FALSE(l.interleaved());
        EXPECT_EQ(2, l.pixelStride());
        EXPECT_EQ(16, l.rowStride());
        int combinations = 0;
        for (int c = 0; c < 2; ++c)
            for (int z = 0; z < 3; ++z)
                for (int t = 0; t < 2; ++t)
                    for (int m = 0; m < 4; ++m) {
                        EXPECT_EQ(c * s.c + z * s.z + t * s.t + m * s.m, l.planeOffset(c, z, t, m))
                            << "c=" << c << " z=" << z << " t=" << t << " m=" << m;
                        ++combinations;
                    }
        EXPECT_EQ(48, combinations);
    }
}

TEST(LifLayout, StorageIndexFollowsStrides)
{
    const auto v = loadFixture("nesting.lif");
    const Layout czmt = buildNamed(v, "Nest/CZMT");
    EXPECT_EQ(0, czmt.storageIndex(0, 0, 0, 0));
    EXPECT_EQ(1, czmt.storageIndex(1, 0, 0, 0));
    EXPECT_EQ(2, czmt.storageIndex(0, 1, 0, 0));
    EXPECT_EQ(6, czmt.storageIndex(0, 0, 0, 1));
    EXPECT_EQ(24, czmt.storageIndex(0, 0, 1, 0));
    EXPECT_EQ(47, czmt.storageIndex(1, 2, 1, 3));

    // Storage index k is the k-th plane by offset, for every nesting.
    for (const char* name : {"Nest/CZMT", "Nest/ZCMT", "Nest/MCZT", "Nest/CZTM"}) {
        SCOPED_TRACE(name);
        const Layout l = buildNamed(v, name);
        for (int c = 0; c < 2; ++c)
            for (int z = 0; z < 3; ++z)
                for (int t = 0; t < 2; ++t)
                    for (int m = 0; m < 4; ++m) {
                        EXPECT_EQ(static_cast<int64_t>(l.planeOffset(c, z, t, m) / 96),
                                  l.storageIndex(c, z, t, m));
                    }
    }
}

TEST(LifLayout, Types)
{
    const auto v = loadFixture("types.lif");
    EXPECT_EQ(DataType::DT_Byte, buildNamed(v, "U8").dataType());
    const Layout u16 = buildNamed(v, "U16_12bit");
    EXPECT_EQ(DataType::DT_UInt16, u16.dataType());
    EXPECT_EQ(2, u16.sampleBytes());
    EXPECT_EQ(12, u16.significantBits(0));
    EXPECT_EQ(DataType::DT_UInt32, buildNamed(v, "U32").dataType());
    EXPECT_EQ(DataType::DT_Float16, buildNamed(v, "F16").dataType());
    EXPECT_EQ(DataType::DT_Float32, buildNamed(v, "F32").dataType());
    const Layout f64 = buildNamed(v, "F64");
    EXPECT_EQ(DataType::DT_Float64, f64.dataType());
    EXPECT_EQ(8, f64.sampleBytes());
    EXPECT_EQ(64, f64.significantBits(0));
    EXPECT_EQ(DataType::DT_UInt16, buildNamed(v, "NoDataType16").dataType());
    expectUnsupported([&] { buildNamed(v, "Mismatch"); }, "BytesInc");
}

TEST(LifLayout, InterleavedRgb)
{
    const auto v = loadFixture("rgb.lif");
    struct Case { const char* name; int r, g, b; };
    for (const Case& k : {Case{"TagRGB", 0, 1, 2}, Case{"TagBGR", 2, 1, 0}, Case{"Untagged", 2, 1, 0}}) {
        SCOPED_TRACE(k.name);
        const Layout l = buildNamed(v, k.name);
        EXPECT_TRUE(l.interleaved());
        EXPECT_EQ(3, l.numChannels());
        EXPECT_EQ(DataType::DT_Byte, l.dataType());
        EXPECT_EQ(1, l.sampleBytes());
        EXPECT_EQ(3, l.pixelStride());
        EXPECT_EQ(24, l.rowStride());
        EXPECT_EQ(k.r, l.storedSample(0));
        EXPECT_EQ(k.g, l.storedSample(1));
        EXPECT_EQ(k.b, l.storedSample(2));
        // The channel term is the stored sample's offset within the pixel.
        EXPECT_EQ(static_cast<uint64_t>(k.r), l.planeOffset(0, 0, 0, 0));
        EXPECT_EQ(static_cast<uint64_t>(k.b), l.planeOffset(2, 0, 0, 0));
        // One stored plane for all three samples.
        EXPECT_EQ(0, l.storageIndex(0, 0, 0, 0));
        EXPECT_EQ(0, l.storageIndex(2, 0, 0, 0));
    }
}

TEST(LifLayout, ScalarStoredSampleIsIdentity)
{
    const auto v = loadFixture("nesting.lif");
    const Layout l = buildNamed(v, "Nest/CZMT");
    EXPECT_EQ(0, l.storedSample(0));
    EXPECT_EQ(1, l.storedSample(1));
}

TEST(LifLayout, XzAndXt)
{
    const auto v = loadFixture("xzxt.lif");
    const Layout xz = buildNamed(v, "XZ");
    EXPECT_EQ(16, xz.width());
    EXPECT_EQ(5, xz.height());
    EXPECT_EQ(1, xz.numZ());
    EXPECT_EQ(1, xz.numT());
    EXPECT_EQ(16, xz.rowStride());
    EXPECT_NEAR(5e-6 / 4, xz.calibration().y, 1e-15);
    EXPECT_NEAR(1.5e-5 / 15, xz.calibration().x, 1e-15);

    const Layout xt = buildNamed(v, "XT");
    EXPECT_EQ(7, xt.height());
    EXPECT_EQ(1, xt.numT());
    EXPECT_EQ(1, xt.numZ());
    EXPECT_EQ(0, xt.calibration().y);
}

TEST(LifLayout, ExtraAxes)
{
    const auto v = loadFixture("extra_axes.lif");
    const Layout e1 = buildNamed(v, "Extra1");
    EXPECT_EQ(16, e1.width());
    EXPECT_EQ(12, e1.height());
    EXPECT_EQ(1, e1.numZ());
    expectUnsupported([&] { buildNamed(v, "Lambda"); }, "dimension 9");
    expectUnsupported([&] { buildNamed(v, "Lambda"); }, "12");
    EXPECT_NO_THROW(buildNamed(v, "Plain"));
}

TEST(LifLayout, Padding)
{
    const auto v = loadFixture("padded.lif");
    const Layout l = buildNamed(v, "Padded");
    EXPECT_EQ(20, l.rowStride());
    EXPECT_EQ(1, l.pixelStride());
}

TEST(LifLayout, RejectsAliasAndOutOfBounds)
{
    const auto v = loadFixture("bad_strides.lif");
    expectUnsupported([&] { buildNamed(v, "Alias"); }, "overlap");
    expectUnsupported([&] { buildNamed(v, "OOB"); }, "payload");
    EXPECT_NO_THROW(buildNamed(v, "Plain"));
}

TEST(LifLayout, RejectsUnevenChannelIncrements)
{
    ImageDescription d;
    d.dimensions = {dim(1, 8, 1), dim(2, 6, 8)};   // plane 48 bytes
    d.channels = {channel(0), channel(96), channel(288)};
    expectUnsupported([&] { Layout::build(d, 1000); }, "evenly");
}

TEST(LifLayout, RejectsShapeErrors)
{
    {
        ImageDescription d = plainImage(4, 4);
        d.dimensions[0].dimId = 2;
        std::swap(d.dimensions[0], d.dimensions[1]);   // Y first
        expectUnsupported([&] { Layout::build(d, 16); }, "X");
    }
    {
        ImageDescription d = plainImage(4, 4);
        d.dimensions.push_back(dim(3, 2, 16));
        d.dimensions.push_back(dim(3, 2, 32));
        expectUnsupported([&] { Layout::build(d, 64); }, "repeated");
    }
    {
        ImageDescription d = plainImage(4, 4);
        d.dimensions[1].count = 0;
        expectUnsupported([&] { Layout::build(d, 16); }, "elements");
    }
    {
        ImageDescription d = plainImage(4, 4);
        d.dimensions[1].bytesInc = 3;                   // row shorter than W * X
        expectUnsupported([&] { Layout::build(d, 16); }, "row");
    }
    {
        ImageDescription d = plainImage(4, 4);
        d.channels[0].dataType = 2;
        expectUnsupported([&] { Layout::build(d, 16); }, "DataType 2");
    }
}

TEST(LifLayout, OffsetsAbove4GiB)
{
    ImageDescription d;
    d.dimensions = {dim(1, 65536, 1), dim(2, 65536, 65536), dim(3, 2, int64_t(1) << 32)};
    d.channels = {channel(0)};
    const Layout l = Layout::build(d, uint64_t(1) << 33);
    EXPECT_EQ(2, l.numZ());
    EXPECT_EQ(4294967296ULL, l.planeOffset(0, 1, 0, 0));
    // One byte less and the second plane does not fit.
    expectUnsupported([&] { Layout::build(d, (uint64_t(1) << 33) - 1); }, "payload");
}

TEST(LifLayout, RejectsOverflow)
{
    ImageDescription d = plainImage(16, 12);
    d.dimensions.push_back(dim(3, 8, int64_t(1) << 62));
    expectUnsupported([&] { Layout::build(d, ~uint64_t(0)); }, "overflow");
}

TEST(LifLayout, Calibration)
{
    {
        const auto v = loadFixture("v2_basic.lif");
        const Calibration c = buildNamed(v, "Plain").calibration();
        EXPECT_NEAR(1e-6, c.x, 1e-15);
        EXPECT_NEAR(1e-6, c.y, 1e-15);
        EXPECT_EQ(0, c.z);
        EXPECT_EQ(0, c.t);
    }
    {
        const auto v = loadFixture("nesting.lif");
        const Calibration c = buildNamed(v, "Nest/CZMT").calibration();
        EXPECT_NEAR(2e-6, c.z, 1e-15);
        EXPECT_NEAR(10, c.t, 1e-12);
    }
    {
        ImageDescription d = plainImage(16, 12);
        d.dimensions[0].unit = "um";
        const Calibration c = Layout::build(d, 16 * 12).calibration();
        EXPECT_EQ(0, c.x);
        EXPECT_NEAR(1e-6, c.y, 1e-15);
    }
    {
        ImageDescription d = plainImage(16, 1);
        const Calibration c = Layout::build(d, 16).calibration();
        EXPECT_EQ(0, c.y);
    }
    {
        // T in kiloseconds, and the scanner delay as the fallback.
        ImageDescription d = plainImage(4, 4);
        d.dimensions.push_back(dim(4, 3, 16, 0.002, "Ks"));
        EXPECT_NEAR(1, Layout::build(d, 48).calibration().t, 1e-12);
        d.dimensions[2].unit = "";
        d.delayTimeSeconds = 2.5;
        EXPECT_NEAR(2.5, Layout::build(d, 48).calibration().t, 1e-12);
    }
}
