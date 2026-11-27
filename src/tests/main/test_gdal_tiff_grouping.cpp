// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#include "slideio/drivers/gdal/gdaltiffgrouping.hpp"
#include "slideio/imagetools/smalltiffwrapper.hpp"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace slideio;

namespace
{
    TiffDirectory dir(int w, int h, int channels = 3, DataType dt = DataType::DT_Byte,
                      int bits = 8, int subFileType = 0)
    {
        TiffDirectory d;
        d.width = w;
        d.height = h;
        d.channels = channels;
        d.dataType = dt;
        d.bitsPerSample = bits;
        d.subFileType = subFileType;
        return d;
    }

    std::vector<int> widths(const GDALTiffSceneGroup& g)
    {
        std::vector<int> w;
        for (const auto& l : g.levels) w.push_back(l.width);
        return w;
    }
}

TEST(GDALTiffPyramid, plainPagesStayScenes)
{
    std::vector<TiffDirectory> dirs = { dir(256, 256), dir(256, 256), dir(256, 256) };
    for (int i = 0; i < 3; ++i) dirs[i].dirIndex = i;
    auto groups = groupTiffDirectories(dirs);
    ASSERT_EQ(3u, groups.size());
    for (int i = 0; i < 3; ++i)
    {
        EXPECT_EQ(i, groups[i].firstDirectory);
        EXPECT_EQ(1u, groups[i].levels.size());
    }
}

TEST(GDALTiffPyramid, subIfdsBecomeLevels)
{
    TiffDirectory base = dir(1000, 800);
    base.subdirectories = { dir(500, 400), dir(250, 200) };
    auto groups = groupTiffDirectories({ base });
    ASSERT_EQ(1u, groups.size());
    EXPECT_EQ((std::vector<int>{1000, 500, 250}), widths(groups[0]));
}

TEST(GDALTiffPyramid, reducedIfdsBecomeLevels)
{
    auto groups = groupTiffDirectories({ dir(1000, 800), dir(500, 400, 3, DataType::DT_Byte, 8, 1),
                                         dir(250, 200, 3, DataType::DT_Byte, 8, 1) });
    ASSERT_EQ(1u, groups.size());
    EXPECT_EQ(3u, groups[0].levels.size());
}

TEST(GDALTiffPyramid, masksAreSkipped)
{
    TiffDirectory maskSub = dir(600, 480, 3, DataType::DT_Byte, 8, 4);
    TiffDirectory base = dir(1000, 800);
    base.subdirectories = { maskSub };
    auto groups = groupTiffDirectories({ base,
                                         dir(500, 400, 3, DataType::DT_Byte, 8, 1),
                                         dir(500, 400, 3, DataType::DT_Byte, 8, 4),
                                         dir(500, 400, 3, DataType::DT_Byte, 8, 5) });
    ASSERT_EQ(1u, groups.size());
    EXPECT_EQ((std::vector<int>{1000, 500}), widths(groups[0]));
}

TEST(GDALTiffPyramid, onlyMasksFallBackToScenes)
{
    auto groups = groupTiffDirectories({ dir(100, 100, 3, DataType::DT_Byte, 8, 4),
                                         dir(100, 100, 3, DataType::DT_Byte, 8, 4) });
    EXPECT_EQ(2u, groups.size());
}

TEST(GDALTiffPyramid, mismatchedReducedPageIsOwnSceneAndDoesNotEndRun)
{
    std::vector<TiffDirectory> dirs = {
        dir(1000, 800, 3),
        dir(500, 400, 3, DataType::DT_Byte, 8, 1),
        dir(500, 400, 1, DataType::DT_Byte, 8, 1),
        dir(250, 200, 3, DataType::DT_Byte, 8, 1) };
    for (int i = 0; i < 4; ++i) dirs[i].dirIndex = i;
    auto groups = groupTiffDirectories(dirs);
    ASSERT_EQ(2u, groups.size());
    EXPECT_EQ(0, groups[0].firstDirectory);
    EXPECT_EQ((std::vector<int>{1000, 500, 250}), widths(groups[0]));
    EXPECT_EQ(2, groups[1].firstDirectory);
    EXPECT_EQ(1u, groups[1].levels.size());
}

TEST(GDALTiffPyramid, aspectRatioMismatchIsNotALevel)
{
    auto groups = groupTiffDirectories({ dir(1000, 800), dir(300, 300, 3, DataType::DT_Byte, 8, 1) });
    EXPECT_EQ(2u, groups.size());
    groups = groupTiffDirectories({ dir(1001, 701),
                                    dir(501, 351, 3, DataType::DT_Byte, 8, 1),
                                    dir(251, 176, 3, DataType::DT_Byte, 8, 1) });
    ASSERT_EQ(1u, groups.size());
    EXPECT_EQ(3u, groups[0].levels.size());
}

TEST(GDALTiffPyramid, notSmallerIsNotALevel)
{
    auto groups = groupTiffDirectories({ dir(1000, 800), dir(1000, 800, 3, DataType::DT_Byte, 8, 1) });
    EXPECT_EQ(2u, groups.size());
}

TEST(GDALTiffPyramid, sizeTieKeepsSubIfd)
{
    TiffDirectory sub = dir(500, 400);
    sub.description = "sub";
    TiffDirectory base = dir(1000, 800);
    base.subdirectories = { sub };
    TiffDirectory top = dir(500, 400, 3, DataType::DT_Byte, 8, 1);
    top.description = "top";
    auto groups = groupTiffDirectories({ base, top });
    ASSERT_EQ(1u, groups.size());
    ASSERT_EQ(2u, groups[0].levels.size());
    EXPECT_EQ("sub", groups[0].levels[1].description);
}

TEST(GDALTiffPyramid, reducedBeforeAnyBaseIsOwnScene)
{
    auto groups = groupTiffDirectories({ dir(500, 400, 3, DataType::DT_Byte, 8, 1) });
    ASSERT_EQ(1u, groups.size());
    EXPECT_EQ(1u, groups[0].levels.size());
}

TEST(GDALTiffPyramid, twoPyramidsInOneFile)
{
    auto groups = groupTiffDirectories({ dir(1000, 800), dir(500, 400, 3, DataType::DT_Byte, 8, 1),
                                         dir(600, 600), dir(300, 300, 3, DataType::DT_Byte, 8, 1) });
    ASSERT_EQ(2u, groups.size());
    EXPECT_EQ(2u, groups[0].levels.size());
    EXPECT_EQ(2u, groups[1].levels.size());
}

TEST(GDALTiffPyramid, directoryMetadataMatchesPage)
{
    TiffDirectory tiled = dir(512, 256);
    tiled.tiled = true;
    tiled.tileWidth = 256;
    tiled.tileHeight = 128;
    tiled.interleaved = true;
    tiled.slideioCompression = Compression::Jpeg;
    tiled.description = "desc";
    tiled.software = "sw";
    auto mt = nlohmann::json::parse(SmallTiffWrapper::directoryMetadata(tiled));
    EXPECT_EQ(512, mt["width"].get<int>());
    EXPECT_EQ(256, mt["height"].get<int>());
    EXPECT_TRUE(mt["tiled"].get<bool>());
    EXPECT_EQ(256, mt["tileWidth"].get<int>());
    EXPECT_EQ(128, mt["tileHeight"].get<int>());
    EXPECT_EQ(3, mt["channels"].get<int>());
    EXPECT_TRUE(mt.contains("dataType"));
    EXPECT_EQ(compressionToString(Compression::Jpeg), mt["compression"].get<std::string>());
    EXPECT_EQ("desc", mt["description"].get<std::string>());
    EXPECT_EQ("sw", mt["software"].get<std::string>());
    EXPECT_FALSE(mt.contains("rowsPerStrip"));

    TiffDirectory striped = dir(512, 256);
    striped.rowsPerStrip = 16;
    striped.stripSize = 2048;
    striped.slideioCompression = Compression::Uncompressed;
    auto st = nlohmann::json::parse(SmallTiffWrapper::directoryMetadata(striped));
    EXPECT_FALSE(st["tiled"].get<bool>());
    EXPECT_EQ(16, st["rowsPerStrip"].get<int>());
    EXPECT_EQ(2048, st["stripSize"].get<int>());
    EXPECT_FALSE(st.contains("tileWidth"));
    EXPECT_FALSE(st.contains("description"));
    EXPECT_FALSE(st.contains("software"));
}
