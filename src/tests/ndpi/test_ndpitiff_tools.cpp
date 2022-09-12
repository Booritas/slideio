#include <gtest/gtest.h>
#include "slideio/drivers/ndpi/ndpitifftools.hpp"
#include "tests/testlib/testtools.hpp"
#include <string>

TEST(NDPITiffTools, extractMagnification)
{
    std::string filePath = TestTools::getTestImagePath("svs", "JP2K-33003-1.svs");
    std::vector<slideio::NDPITiffDirectory> dirs;
    slideio::NDPITiffTools::scanFile(filePath, dirs);
    int dirCount = (int)dirs.size();
    ASSERT_EQ(dirCount, 6);
    const slideio::NDPITiffDirectory& dir = dirs[0];
    EXPECT_EQ(dir.width, 15374);
    EXPECT_EQ(dir.height, 17497);
    EXPECT_TRUE(dir.tiled);
    EXPECT_EQ(dir.tileWidth, 256);
    EXPECT_EQ(dir.tileHeight, 256);
    EXPECT_EQ(dir.channels, 3);
    EXPECT_EQ(dir.bitsPerSample, 8);
    EXPECT_EQ(dir.description.size(), 530);
    const slideio::NDPITiffDirectory& dir5 = dirs[5];
    EXPECT_EQ(dir5.width, 1280);
    EXPECT_EQ(dir5.height, 421);
    EXPECT_FALSE(dir5.tiled);
    EXPECT_EQ(dir5.tileWidth, 0);
    EXPECT_EQ(dir5.tileHeight, 0);
    EXPECT_EQ(dir5.channels, 3);
    EXPECT_EQ(dir5.bitsPerSample, 8);
    EXPECT_EQ(dir5.description.size(), 44);
    EXPECT_TRUE(dir5.interleaved);
    EXPECT_EQ(0, dir5.res.x);
    EXPECT_EQ(0, dir5.res.y);
    EXPECT_EQ((uint32_t)7, dir5.compression);
}

