// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <string>
#include "slideio/drivers/nd2/nd2attributes.hpp"
#include "nd2_test_trees.hpp"

using namespace slideio;
using namespace slideio::nd2;

namespace
{
    Attributes attrsOf(const char* name)
    {
        return Attributes::parse(nd2test::loadTrees(name).attributes);
    }

    std::vector<ChannelInfo> channelsOf(const char* name)
    {
        const nd2test::Trees t = nd2test::loadTrees(name);
        const Attributes a = Attributes::parse(t.attributes);
        return parseChannels(t.hasPicture ? &t.picture : nullptr, a.components);
    }

    std::string refusal(const char* name)
    {
        try {
            attrsOf(name);
        }
        catch (const Unsupported& e) {
            return e.what();
        }
        return "";
    }
}

TEST(Nd2Attributes, Basic)
{
    const Attributes a = attrsOf("v3_basic.nd2");
    EXPECT_EQ(a.width, 16);
    EXPECT_EQ(a.height, 12);
    EXPECT_EQ(a.components, 1);
    EXPECT_EQ(a.dataType(), DataType::DT_Byte);
    EXPECT_EQ(a.sampleBytes(), 1);
    EXPECT_EQ(a.rowStride, 16);
    EXPECT_EQ(a.compression, FrameCompression::None);
}

TEST(Nd2Attributes, PaddedStride)
{
    const Attributes a = attrsOf("v21_xml.nd2");
    EXPECT_EQ(a.components, 3);
    EXPECT_EQ(a.width, 15);
    EXPECT_EQ(a.rowStride, 48);
}

TEST(Nd2Attributes, Types)
{
    const Attributes tpz = attrsOf("v3_tpz.nd2");
    EXPECT_EQ(tpz.dataType(), DataType::DT_UInt16);
    EXPECT_EQ(tpz.sampleBytes(), 2);
    EXPECT_EQ(tpz.bitsInMemory, 16);
    EXPECT_EQ(tpz.significantBits, 12);
    EXPECT_EQ(tpz.sequenceCount, 24);
    const Attributes f = attrsOf("float32.nd2");
    EXPECT_EQ(f.dataType(), DataType::DT_Float32);
    EXPECT_EQ(f.sampleBytes(), 4);
    EXPECT_EQ(attrsOf("v20_zlib.nd2").compression, FrameCompression::Zlib);
}

TEST(Nd2Attributes, Refusals)
{
    EXPECT_NE(refusal("bad_width.nd2").find("uiWidth"), std::string::npos);
    EXPECT_NE(refusal("lossy.nd2").find("eCompression"), std::string::npos);
    EXPECT_NE(refusal("tiled.nd2").find("uiTileWidth"), std::string::npos);
    EXPECT_THROW(attrsOf("bad_width.nd2"), Unsupported);
    EXPECT_THROW(attrsOf("lossy.nd2"), Unsupported);
    EXPECT_THROW(attrsOf("tiled.nd2"), Unsupported);
}

TEST(Nd2Attributes, NotAMap)
{
    EXPECT_THROW(Attributes::parse(Variant()), Unsupported);
}

TEST(Nd2Channels, MonoAndRgb)
{
    const auto tpz = channelsOf("v3_tpz.nd2");
    ASSERT_EQ(tpz.size(), 2u);
    EXPECT_EQ(tpz[0].name, "DAPI");
    EXPECT_EQ(colorToHex(tpz[0].color), "#FF0000");
    EXPECT_EQ(tpz[1].name, "GFP");
    EXPECT_EQ(colorToHex(tpz[1].color), "#00FF00");
    EXPECT_EQ(tpz[1].nd2Channel, 1);

    const auto rgb = channelsOf("v21_xml.nd2");
    ASSERT_EQ(rgb.size(), 3u);
    const char* suffix[] = { " R", " G", " B" };
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(rgb[i].nd2Channel, 0);
        EXPECT_EQ(rgb[i].component, i);
        ASSERT_GT(rgb[i].name.size(), 2u);
        EXPECT_EQ(rgb[i].name.substr(rgb[i].name.size() - 2), suffix[i]);
    }
}

TEST(Nd2Channels, MultiRgb)
{
    const auto ch = channelsOf("multi_rgb.nd2");
    ASSERT_EQ(ch.size(), 12u);
    EXPECT_EQ(ch[7].name, "BF3 G");
    EXPECT_EQ(ch[7].nd2Channel, 2);
    EXPECT_EQ(ch[7].component, 1);
    EXPECT_EQ(ch[0].name, "BF1 R");
    EXPECT_EQ(ch[11].name, "BF4 B");
}

TEST(Nd2Channels, BadDivision)
{
    EXPECT_THROW(channelsOf("bad_comp.nd2"), Unsupported);
}

TEST(Nd2Channels, NoPictureMetadata)
{
    const auto ch = parseChannels(nullptr, 2);
    ASSERT_EQ(ch.size(), 2u);
    EXPECT_TRUE(ch[0].name.empty());
    EXPECT_TRUE(ch[1].name.empty());
    EXPECT_EQ(ch[1].nd2Channel, 1);
    EXPECT_EQ(ch[1].component, 0);
}

TEST(Nd2Channels, ColorHex)
{
    EXPECT_EQ(colorToHex(0x00FF8040), "#4080FF");   // packed 0x00BBGGRR
    EXPECT_EQ(colorToHex(0xFFFFFF), "#FFFFFF");
    EXPECT_EQ(colorToHex(0), "#000000");
}

namespace
{
    /// An attributes tree with the given values, as CLX XML decodes it.
    Variant attributesTree(const std::string& width, const std::string& comp, const std::string& bits,
        const std::string& widthBytes)
    {
        return decodeClxXml(
            "<variant><uiWidth runtype=\"lx_uint32\" value=\"" + width + "\"/>"
            "<uiHeight runtype=\"lx_uint32\" value=\"4\"/>"
            "<uiComp runtype=\"lx_uint32\" value=\"" + comp + "\"/>"
            "<uiBpcInMemory runtype=\"lx_uint32\" value=\"" + bits + "\"/>"
            "<uiWidthBytes runtype=\"lx_uint64\" value=\"" + widthBytes + "\"/></variant>");
    }

    std::string parseRefusal(const Variant& tree)
    {
        try {
            Attributes::parse(tree);
        }
        catch (const Unsupported& e) {
            return e.what();
        }
        return "";
    }
}

TEST(Nd2Attributes, ComponentLimitAndRowSize)
{
    // uiComp beyond CV_CN_MAX (512) is refused by parse itself, naming the field, before
    // parseChannels reserves anything by it.
    const std::string many = parseRefusal(attributesTree("16", "2147483647", "8", "34359738352"));
    EXPECT_NE(many.find("uiComp is 2147483647"), std::string::npos) << many;
    EXPECT_NE(parseRefusal(attributesTree("16", "513", "8", "8208")).find("uiComp"), std::string::npos);
    EXPECT_THROW(parseChannels(nullptr, 513), Unsupported);
    EXPECT_EQ(Attributes::parse(attributesTree("16", "512", "8", "8192")).components, 512);

    // The largest row the limits allow: INT_MAX pixels of 512 4-byte components.
    const Attributes wide = Attributes::parse(attributesTree("2147483647", "512", "32", "4398046509056"));
    EXPECT_EQ(wide.rowStride, 4398046509056LL);
    const std::string shortRow = parseRefusal(attributesTree("2147483647", "512", "32", "4398046509055"));
    EXPECT_NE(shortRow.find("uiWidthBytes is 4398046509055 but a packed row needs 4398046509056"),
        std::string::npos) << shortRow;
}
