// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <tinyxml2.h>
#include <string>
#include <vector>
#include "slideio/drivers/lif/liftimestamps.hpp"
#include "slideio/drivers/lif/liflayout.hpp"
#include "slideio/drivers/lif/lifxml.hpp"
#include "slideio/drivers/lif/lifcontainer.hpp"
#include "slideio/core/tools/filereader.hpp"

using namespace slideio;
using namespace slideio::lif;

namespace
{
    constexpr uint64_t kBase = 131000000000000000ULL;
    constexpr uint64_t kStep = 10000000ULL;

    struct Loaded
    {
        ImageDescription description;
        uint64_t payloadLength = 0;
    };

    Loaded load(const char* file, const std::string& path)
    {
        FileReader reader(std::string(LIF_TEST_DATA_DIR) + "/" + file);
        const Container c = Container::read(reader);
        for (auto& d : parseImages(c.xml())) {
            if (d.path == path) {
                Loaded l;
                const MemoryBlock* block = c.findBlock(d.memoryId);
                l.payloadLength = block ? block->length : 0;
                l.description = std::move(d);
                return l;
            }
        }
        throw std::runtime_error("no image " + path);
    }

    void checkFixture(const std::string& name, bool expectComplete)
    {
        const Loaded l = load("timestamps.lif", name);
        const Layout layout = Layout::build(l.description, l.payloadLength);
        const PlaneTimes times = PlaneTimes::build(l.description.timestampTicks, layout, 0);
        ASSERT_EQ(times.complete(), expectComplete);
        if (!expectComplete) {
            EXPECT_EQ(times.offset(0, 0, 0), 0.0);
            return;
        }
        EXPECT_EQ(times.acquisitionTime(), 1455526400);
        for (int t = 0; t < 3; ++t) {
            for (int c = 0; c < 2; ++c) {
                EXPECT_EQ(times.offset(t, c, 0), static_cast<double>(layout.storageIndex(c, 0, t, 0)));
            }
        }
    }

    std::vector<uint64_t> parseXml(const char* xml)
    {
        tinyxml2::XMLDocument doc;
        EXPECT_EQ(doc.Parse(xml), tinyxml2::XML_SUCCESS);
        return parseTimestampTicks(doc.RootElement());
    }
}

TEST(LifTimestamps, HexEncoding)
{
    checkFixture("Hex", true);
}

TEST(LifTimestamps, DecimalEncodingMatchesHex)
{
    checkFixture("Decimal", true);
    const Loaded h = load("timestamps.lif", "Hex");
    const Loaded d = load("timestamps.lif", "Decimal");
    EXPECT_EQ(h.description.timestampTicks, d.description.timestampTicks);
    EXPECT_EQ(h.description.timestampTicks.size(), 6u);
}

TEST(LifTimestamps, ShortListIsIncomplete)
{
    checkFixture("Short", false);
}

TEST(LifTimestamps, SignedLowWordMasked)
{
    const auto ticks = parseXml(
        "<Image><TimeStampList><TimeStamp HighInteger=\"30500000\" LowInteger=\"-1\"/></TimeStampList></Image>");
    ASSERT_EQ(ticks.size(), 1u);
    EXPECT_EQ(ticks[0], (30500000ULL << 32) | 0xFFFFFFFFULL);
}

TEST(LifTimestamps, HexWordSplit)
{
    const auto ticks = parseXml("<Image><TimeStampList> 1d1a2b3c4d5e6f70  FF </TimeStampList></Image>");
    ASSERT_EQ(ticks.size(), 2u);
    EXPECT_EQ(ticks[0], 0x1d1a2b3c4d5e6f70ULL);
    EXPECT_EQ(ticks[1], 0xFFULL);
}

TEST(LifTimestamps, MalformedListIsAbsent)
{
    EXPECT_TRUE(parseXml("<Image><TimeStampList>10 zz 30</TimeStampList></Image>").empty());
    EXPECT_TRUE(parseXml("<Image><TimeStampList>1ffffffffffffffff</TimeStampList></Image>").empty());
    EXPECT_TRUE(parseXml("<Image><TimeStampList><TimeStamp HighInteger=\"1\"/></TimeStampList></Image>").empty());
    EXPECT_TRUE(parseXml("<Image/>").empty());
    EXPECT_TRUE(parseTimestampTicks(nullptr).empty());
}

TEST(LifTimestamps, PerFieldOrigin)
{
    const Loaded l = load("nesting.lif", "Nest/CZTM");
    const Layout layout = Layout::build(l.description, l.payloadLength);
    std::vector<uint64_t> ticks;
    for (uint64_t k = 0; k < 48; ++k) {
        ticks.push_back(kBase + k * kStep);
    }
    const PlaneTimes f0 = PlaneTimes::build(ticks, layout, 0);
    const PlaneTimes f1 = PlaneTimes::build(ticks, layout, 1);
    ASSERT_TRUE(f0.complete());
    ASSERT_TRUE(f1.complete());
    EXPECT_EQ(f1.acquisitionTime() - f0.acquisitionTime(), 12);
    // Whole-second ticks: the floored origin is the earliest tick.
    EXPECT_EQ(f1.offset(0, 0, 0), 0.0);
    for (int t = 0; t < 2; ++t) {
        for (int z = 0; z < 3; ++z) {
            for (int c = 0; c < 2; ++c) {
                EXPECT_GE(f1.offset(t, c, z), 0.0);
            }
        }
    }
}

TEST(LifTimestamps, SubSecondKeepsContract)
{
    const Loaded l = load("timestamps.lif", "Hex");
    const Layout layout = Layout::build(l.description, l.payloadLength);
    std::vector<uint64_t> ticks;
    for (uint64_t k = 0; k < 6; ++k) {
        ticks.push_back(kBase + 2500000ULL + k * kStep);   // .25 s
    }
    const PlaneTimes times = PlaneTimes::build(ticks, layout, 0);
    ASSERT_TRUE(times.complete());
    EXPECT_EQ(times.acquisitionTime(), 1455526400);
    for (int t = 0; t < 3; ++t) {
        for (int c = 0; c < 2; ++c) {
            EXPECT_NEAR(times.offset(t, c, 0), 0.25 + layout.storageIndex(c, 0, t, 0), 1e-9);
        }
    }
}

TEST(LifTimestamps, ExtraTicksAreIncomplete)
{
    const Loaded l = load("timestamps.lif", "Hex");
    const Layout layout = Layout::build(l.description, l.payloadLength);
    std::vector<uint64_t> ticks(7, kBase);
    EXPECT_FALSE(PlaneTimes::build(ticks, layout, 0).complete());
}
