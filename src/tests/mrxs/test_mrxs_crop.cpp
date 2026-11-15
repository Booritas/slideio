// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// mrxs.crop_to_content: the crop box and the cropped scene (open options design,
// section 7), on the committed fixtures. offset-content's content starts at (27, 9),
// which no coarser downsample divides, so every level origin is non-zero.
#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <string>
#include "slideio/drivers/mrxs/mrxsdataset.hpp"
#include "slideio/drivers/mrxs/mrxsgeometry.hpp"
#include "slideio/core/exceptions.hpp"

namespace fs = std::filesystem;
using namespace slideio::mrxs;

namespace
{
    const fs::path dataDir() { return fs::path(MRXS_TEST_DATA_DIR); }

    std::string entry(const std::string& name) { return (dataDir() / (name + ".mrxs")).u8string(); }
}

TEST(MRXSCropBox, isTheContentBoundsWhenTheyLieInsideTheCanvas)
{
    const Geometry g = MRXSDataset::open(entry("offset-content"))->geometry;
    const auto box = computeCropBox(g);
    ASSERT_TRUE(box.has_value());
    EXPECT_EQ(cv::Rect(27, 9, 45, 45), box->level0);
    ASSERT_EQ(g.levels.size(), box->levels.size());
    EXPECT_EQ(box->level0, box->levels[0]);
    auto expectLevel = [&](size_t l, int64_t d, int x0, int y0, int64_t cx1, int64_t cy1) {
        const int x1 = static_cast<int>(std::min(cx1, g.levels[l].width));
        const int y1 = static_cast<int>(std::min(cy1, g.levels[l].height));
        EXPECT_EQ(d, g.levels[l].downsample);
        EXPECT_EQ(cv::Rect(x0, y0, x1 - x0, y1 - y0), box->levels[l]) << "level " << l;
    };
    expectLevel(1, 2, 13, 4, 36, 27);   // floor(27/2), floor(9/2), ceil(72/2), ceil(54/2)
    expectLevel(2, 4, 6, 2, 18, 14);    // floor(27/4), floor(9/4), ceil(72/4), ceil(54/4)
}

TEST(MRXSCropBox, isClippedToTheCanvas)
{
    const Geometry g = MRXSDataset::open(entry("rawmap-d2"))->geometry;    // bounds (-3, -2, 93, 70)
    const auto box = computeCropBox(g);
    ASSERT_TRUE(box.has_value());
    EXPECT_EQ(cv::Rect(0, 0, static_cast<int>(std::min<int64_t>(90, g.levels[0].width)),
                       static_cast<int>(std::min<int64_t>(68, g.levels[0].height))), box->level0);
}

TEST(MRXSCropBox, isAbsentWithoutDrawablesOrOutsideTheCanvas)
{
    Geometry g = MRXSDataset::open(entry("nomap-d1"))->geometry;
    g.bounds0.reset();
    EXPECT_FALSE(computeCropBox(g).has_value());
    g.bounds0 = cv::Rect2d(double(g.levels[0].width) + 5, 0, 10, 10);
    EXPECT_FALSE(computeCropBox(g).has_value());
}

TEST(MRXSCropBox, theDatasetCarriesOneOnlyWhenAsked)
{
    const auto plain = MRXSDataset::open(entry("offset-content"));
    EXPECT_FALSE(plain->cropRequested);
    EXPECT_FALSE(plain->crop.has_value());
    const auto cropped = MRXSDataset::open(entry("offset-content"), RuntimeOptions(), true);
    EXPECT_TRUE(cropped->cropRequested);
    ASSERT_TRUE(cropped->crop.has_value());
    EXPECT_EQ(cv::Rect(27, 9, 45, 45), cropped->crop->level0);
}
