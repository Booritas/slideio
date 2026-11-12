// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <string>
#include "slideio/drivers/lif/lifcontainer.hpp"
#include "slideio/core/tools/filereader.hpp"
#include "slideio/core/exceptions.hpp"

using namespace slideio;
using namespace slideio::lif;

namespace
{
    std::string fixture(const char* name)
    {
        return std::string(LIF_TEST_DATA_DIR) + "/" + name;
    }

    Container readFixture(const char* name)
    {
        FileReader file(fixture(name));
        return Container::read(file);
    }

    std::string errorOf(const char* name)
    {
        try {
            readFixture(name);
        }
        catch (const std::exception& ex) {
            return ex.what();
        }
        return "";
    }
}

TEST(LifContainer, ReadsVersion2)
{
    const Container c = readFixture("v2_basic.lif");
    EXPECT_EQ(2, c.version());
    EXPECT_EQ(8, c.lengthFieldWidth());
    ASSERT_EQ(3u, c.blocks().size());
    EXPECT_EQ(0u, c.blocks()[0].length);
    const MemoryBlock* block = c.findBlock("MemBlock_0");
    ASSERT_NE(nullptr, block);
    EXPECT_EQ(16u * 12u, block->length);
    EXPECT_FALSE(block->truncated);
    EXPECT_EQ(1, block->occurrences);
    EXPECT_NE(std::string::npos, c.xml().find("LMSDataContainerHeader"));
    EXPECT_EQ(nullptr, c.findBlock("nope"));
}

TEST(LifContainer, ReadsVersion1)
{
    const Container c = readFixture("v1_basic.lif");
    EXPECT_EQ(1, c.version());
    EXPECT_EQ(4, c.lengthFieldWidth());
    ASSERT_EQ(3u, c.blocks().size());
    EXPECT_EQ(0u, c.blocks()[0].length);
    const MemoryBlock* block = c.findBlock("MemBlock_0");
    ASSERT_NE(nullptr, block);
    EXPECT_EQ(16u * 12u, block->length);
}

TEST(LifContainer, ProbesUnknownVersion)
{
    const Container c = readFixture("v3_probe.lif");
    EXPECT_EQ(3, c.version());
    EXPECT_EQ(8, c.lengthFieldWidth());
    EXPECT_NE(nullptr, c.findBlock("MemBlock_0"));
}

TEST(LifContainer, DuplicateIdCounted)
{
    const Container c = readFixture("broken_refs.lif");
    const MemoryBlock* block = c.findBlock("MemBlock_0");
    ASSERT_NE(nullptr, block);
    EXPECT_EQ(2, block->occurrences);
}

TEST(LifContainer, TruncatedBlockMarked)
{
    const Container c = readFixture("truncated.lif");
    ASSERT_GE(c.blocks().size(), 2u);
    EXPECT_TRUE(c.blocks().back().truncated);
    const MemoryBlock* good = c.findBlock("MemBlock_0");
    ASSERT_NE(nullptr, good);
    EXPECT_FALSE(good->truncated);
}

TEST(LifContainer, TrailerGarbageTolerated)
{
    EXPECT_NO_THROW(readFixture("trailer_garbage.lif"));
}

TEST(LifContainer, DecodesUtf16)
{
    const Container c = readFixture("unicode.lif");
    const std::string expected = "Zellkern \xC2\xB5 \xE2\x80\x93 \xE7\xB4\xB0\xE8\x83\x9E";
    EXPECT_NE(std::string::npos, c.xml().find(expected));
}

TEST(LifContainer, RejectsNonLif)
{
    EXPECT_NE(std::string::npos, errorOf("not_lif.lif").find("LIF: not a Leica LIF file"));
}

TEST(LifContainer, RejectsLof)
{
    EXPECT_NE(std::string::npos, errorOf("lof.lif").find("LIF: not a Leica LIF file"));
}

TEST(LifContainer, ReadsEveryTopLevelElement)
{
    // Two top-level elements: the identity comes from whichever is the header.
    for (const char* name : {"multi_root.lif", "multi_root_decl.lif"}) {
        SCOPED_TRACE(name);
        const Container c = readFixture(name);
        EXPECT_EQ(2, c.version());
        EXPECT_NE(nullptr, c.findBlock("MemBlock_1"));
    }
}

TEST(LifContainer, MalformedXmlFails)
{
    const std::string what = errorOf("malformed_xml.lif");
    EXPECT_NE(std::string::npos, what.find("LIF: malformed XML description")) << what;
    // tinyxml2's own diagnosis follows
    EXPECT_NE(std::string::npos, what.find("XML_ERROR")) << what;
}
