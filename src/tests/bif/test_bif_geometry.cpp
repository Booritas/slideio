// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/drivers/bif/bifgeometry.hpp"
#include "slideio/drivers/bif/bifxml.hpp"
#include "slideio/core/exceptions.hpp"

using namespace slideio;
using namespace slideio::bif;

namespace
{
    // Inverse of snakePosition: the one-based tile number of (col,row).
    int tileNumber(int col, int row, int cols, int rows)
    {
        for (int n = 1; n <= cols * rows; ++n) {
            const cv::Point pos = snakePosition(n, cols, rows);
            if (pos.x == col && pos.y == row) {
                return n;
            }
        }
        ADD_FAILURE() << "no tile at (" << col << "," << row << ")";
        return 0;
    }

    Aoi makeAoi(int cols, int rows, int tileWidth = 1024, int tileHeight = 1024)
    {
        Aoi aoi;
        aoi.scanned = true;
        aoi.cols = cols;
        aoi.rows = rows;
        aoi.tileWidth = tileWidth;
        aoi.tileHeight = tileHeight;
        return aoi;
    }

    // Joint between (col,row) and (col+1,row).
    void addHorizontal(Aoi& aoi, int col, int row, double overlapX, double overlapY = 0)
    {
        Joint joint;
        joint.tile1 = tileNumber(col, row, aoi.cols, aoi.rows);
        joint.tile2 = tileNumber(col + 1, row, aoi.cols, aoi.rows);
        joint.axis = JointAxis::Horizontal;
        joint.joined = true;
        joint.confidence = 100;
        joint.overlapX = overlapX;
        joint.overlapY = overlapY;
        aoi.joints.push_back(joint);
    }

    // Joint between (col,row+1) and the upper tile (col,row): row boundary `row`.
    void addVertical(Aoi& aoi, int col, int row, double overlapY, double overlapX = 0)
    {
        Joint joint;
        joint.tile1 = tileNumber(col, row + 1, aoi.cols, aoi.rows);
        joint.tile2 = tileNumber(col, row, aoi.cols, aoi.rows);
        joint.axis = JointAxis::Vertical;
        joint.joined = true;
        joint.confidence = 100;
        joint.overlapX = overlapX;
        joint.overlapY = overlapY;
        aoi.joints.push_back(joint);
    }

    Aoi dp200Aoi()
    {
        Aoi aoi = makeAoi(23, 21);
        for (int row = 0; row < aoi.rows; ++row) {
            for (int col = 0; col < aoi.cols - 1; ++col) {
                const bool wide = col == 3 || col == 7 || col == 11 || col == 15 || col == 19;
                addHorizontal(aoi, col, row, wide ? 24 : 0);
            }
        }
        for (int row = 0; row < aoi.rows - 1; ++row) {
            for (int col = 0; col < aoi.cols; ++col) {
                addVertical(aoi, col, row, 0);
            }
        }
        return aoi;
    }
}

TEST(BIFGeometryOverlaps, dp200Pattern)
{
    const Aoi aoi = dp200Aoi();
    const BoundaryOverlaps overlaps = estimateOverlaps(aoi, std::nullopt, std::nullopt);
    ASSERT_EQ(22u, overlaps.columns.size());
    ASSERT_EQ(20u, overlaps.rows.size());
    EXPECT_EQ(24, overlaps.columns[3]);
    EXPECT_EQ(0, overlaps.columns[0]);
    for (int b = 0; b < 22; ++b) {
        const bool wide = b == 3 || b == 7 || b == 11 || b == 15 || b == 19;
        EXPECT_EQ(wide ? 24 : 0, overlaps.columns[b]) << b;
    }
    for (int value : overlaps.rows) {
        EXPECT_EQ(0, value);
    }

    const AoiGeometry geometry(aoi, overlaps);
    EXPECT_EQ(cv::Size(23 * 1024 - 5 * 24, 21 * 1024), geometry.size());
    EXPECT_EQ(cv::Size(23432, 21504), geometry.size());
    EXPECT_EQ(geometry.columnPositions()[4] + 12, geometry.columnCuts()[3]);
    EXPECT_EQ(0, geometry.columnPositions()[0]);
    EXPECT_EQ(4 * 1024 - 24, geometry.columnPositions()[4]);
    ASSERT_EQ(22u, geometry.columnCuts().size());
    ASSERT_EQ(20u, geometry.rowCuts().size());
}

TEST(BIFGeometryOverlaps, medianIsRobustToOutliers)
{
    Aoi aoi = makeAoi(2, 4);
    addHorizontal(aoi, 0, 0, 110);
    addHorizontal(aoi, 0, 1, 112);
    addHorizontal(aoi, 0, 2, 113);
    addHorizontal(aoi, 0, 3, 187);
    const BoundaryOverlaps overlaps = estimateOverlaps(aoi, std::nullopt, std::nullopt);
    ASSERT_EQ(1u, overlaps.columns.size());
    EXPECT_EQ(113, overlaps.columns[0]);   // lround(112.5)
}

TEST(BIFGeometryOverlaps, boundaryWithoutJointsTakesAoiMedian)
{
    Aoi aoi = makeAoi(4, 1);
    addHorizontal(aoi, 0, 0, 100);
    addHorizontal(aoi, 2, 0, 120);
    const BoundaryOverlaps overlaps = estimateOverlaps(aoi, 30.0, 40.0);
    ASSERT_EQ(3u, overlaps.columns.size());
    EXPECT_EQ(100, overlaps.columns[0]);
    EXPECT_EQ(110, overlaps.columns[1]);
    EXPECT_EQ(120, overlaps.columns[2]);
}

TEST(BIFGeometryOverlaps, aoiWithoutVerticalJointsTakesSlideMedian)
{
    Aoi aoi = makeAoi(2, 3);
    addHorizontal(aoi, 0, 0, 100);
    const BoundaryOverlaps overlaps = estimateOverlaps(aoi, 30.0, 41.5);
    ASSERT_EQ(2u, overlaps.rows.size());
    EXPECT_EQ(42, overlaps.rows[0]);
    EXPECT_EQ(42, overlaps.rows[1]);
    EXPECT_EQ(100, overlaps.columns[0]);
}

TEST(BIFGeometryOverlaps, nothingAtAllGivesZero)
{
    const Aoi aoi = makeAoi(3, 3);
    const BoundaryOverlaps overlaps = estimateOverlaps(aoi, std::nullopt, std::nullopt);
    EXPECT_EQ(std::vector<int>({0, 0}), overlaps.columns);
    EXPECT_EQ(std::vector<int>({0, 0}), overlaps.rows);
    const AoiGeometry geometry(aoi, overlaps);
    EXPECT_EQ(cv::Size(3 * 1024, 3 * 1024), geometry.size());
}

TEST(BIFGeometryOverlaps, singleTileHasNoBoundaries)
{
    const Aoi aoi = makeAoi(1, 1);
    const BoundaryOverlaps overlaps = estimateOverlaps(aoi, 10.0, 10.0);
    EXPECT_TRUE(overlaps.columns.empty());
    EXPECT_TRUE(overlaps.rows.empty());
    const AoiGeometry geometry(aoi, overlaps);
    EXPECT_EQ(cv::Size(1024, 1024), geometry.size());
}

TEST(BIFGeometryOverlaps, clamping)
{
    Aoi aoi = makeAoi(2, 2);
    addHorizontal(aoi, 0, 0, 2000);
    addVertical(aoi, 0, 0, -5);
    const BoundaryOverlaps overlaps = estimateOverlaps(aoi, std::nullopt, std::nullopt);
    EXPECT_EQ(1023, overlaps.columns[0]);
    EXPECT_EQ(0, overlaps.rows[0]);
}

TEST(BIFGeometryOverlaps, slideMedian)
{
    Aoi first = makeAoi(3, 1);
    addHorizontal(first, 0, 0, 10);
    addHorizontal(first, 1, 0, 20);
    Aoi second = makeAoi(2, 2);
    addHorizontal(second, 0, 0, 30);
    addHorizontal(second, 0, 1, 1000);
    addVertical(second, 0, 0, 7);
    Aoi unscanned = makeAoi(2, 1);
    addHorizontal(unscanned, 0, 0, 5000);
    unscanned.scanned = false;
    Aoi unusable = makeAoi(2, 1);
    addHorizontal(unusable, 0, 0, 5000);
    unusable.joints.back().confidence = 0;

    const std::vector<Aoi> aois{first, second, unscanned, unusable};
    const std::optional<double> x = slideMedianOverlap(aois, JointAxis::Horizontal);
    ASSERT_TRUE(x.has_value());
    EXPECT_DOUBLE_EQ(25.0, *x);
    const std::optional<double> y = slideMedianOverlap(aois, JointAxis::Vertical);
    ASSERT_TRUE(y.has_value());
    EXPECT_DOUBLE_EQ(7.0, *y);
    EXPECT_FALSE(slideMedianOverlap({first}, JointAxis::Vertical).has_value());
    EXPECT_FALSE(slideMedianOverlap({}, JointAxis::Horizontal).has_value());
}

TEST(BIFGeometryMaps, fullResolution)
{
    Aoi aoi = makeAoi(2, 1);
    addHorizontal(aoi, 0, 0, 100);
    const AoiGeometry geometry(aoi, estimateOverlaps(aoi, std::nullopt, std::nullopt));
    const int x1 = geometry.columnPositions()[1];
    ASSERT_EQ(924, x1);
    ASSERT_EQ(x1 + 50, geometry.columnCuts()[0]);
    EXPECT_EQ(cv::Size(2048 - 100, 1024), geometry.size());

    const std::vector<int> map = geometry.mapColumns(x1 + 49, 2, 1, 2048);
    ASSERT_EQ(2u, map.size());
    EXPECT_EQ(x1 + 49, map[0]);           // tile 0: storage == output
    EXPECT_EQ(1024 + 50, map[1]);         // tile 1, 50 px into it

    const std::vector<int> all = geometry.mapColumns(0, geometry.size().width, 1, 2048);
    for (int X = 0; X < static_cast<int>(all.size()); ++X) {
        const int expected = X < x1 + 50 ? X : X + 100;
        ASSERT_EQ(expected, all[X]) << X;
    }
    EXPECT_EQ(2047, all.back());

    const std::vector<int> rows = geometry.mapRows(0, 1024, 1, 1024);
    for (int Y = 0; Y < 1024; ++Y) {
        ASSERT_EQ(Y, rows[Y]);
    }
}

TEST(BIFGeometryMaps, originIsAdded)
{
    Aoi aoi = makeAoi(2, 2);
    aoi.originX = 3072;
    aoi.originY = 2048;
    addHorizontal(aoi, 0, 0, 100);
    addVertical(aoi, 0, 0, 60);
    const AoiGeometry geometry(aoi, estimateOverlaps(aoi, std::nullopt, std::nullopt));
    const std::vector<int> cols = geometry.mapColumns(0, 1, 1, 10000);
    EXPECT_EQ(3072, cols[0]);
    const std::vector<int> rows = geometry.mapRows(1024 - 60 + 30, 1, 1, 10000);
    EXPECT_EQ(2048 + 1024 + 30, rows[0]);
}

TEST(BIFGeometryMaps, halfResolution)
{
    Aoi aoi = makeAoi(2, 1);
    addHorizontal(aoi, 0, 0, 100);
    const AoiGeometry geometry(aoi, estimateOverlaps(aoi, std::nullopt, std::nullopt));
    const int cut = geometry.columnCuts()[0];   // 974
    const int count = (geometry.size().width + 1) / 2;
    const std::vector<int> map = geometry.mapColumns(0, count, 2, 1024);
    ASSERT_EQ(static_cast<size_t>(count), map.size());
    for (int X = 0; X < count; ++X) {
        const int X0 = 2 * X;
        const int expected = X0 < cut ? X0 / 2 : (1024 + (X0 - 924)) / 2;
        ASSERT_EQ(expected, map[X]) << X;
    }
    EXPECT_EQ(486, map[486]);           // X0 = 972, tile 0
    EXPECT_EQ(537, map[487]);           // X0 = 974, tile 1: (1024 + 50) / 2
}

TEST(BIFGeometryMaps, coarseLevelStaysInsideStorage)
{
    Aoi aoi = makeAoi(1, 75, 1024, 1360);
    for (int row = 0; row < 74; ++row) {
        addVertical(aoi, 0, row, 40 + row % 3);
    }
    const AoiGeometry geometry(aoi, estimateOverlaps(aoi, std::nullopt, std::nullopt));
    const int d = 32;
    const int count = (geometry.size().height + d - 1) / d;
    const std::vector<int> rows = geometry.mapRows(0, count, d, 3188);
    ASSERT_EQ(static_cast<size_t>(count), rows.size());
    EXPECT_LE(rows.back(), 3187);
    EXPECT_GE(rows.front(), 0);
    for (size_t i = 1; i < rows.size(); ++i) {
        ASSERT_LE(rows[i - 1], rows[i]) << i;
    }

    // Without overlaps the AOI fills the raster: 75 * 1360 / 32 = 3187.5.
    const Aoi abutting = makeAoi(1, 75, 1024, 1360);
    const AoiGeometry flat(abutting, estimateOverlaps(abutting, std::nullopt, std::nullopt));
    const int flatCount = (flat.size().height + d - 1) / d;
    ASSERT_EQ(3188, flatCount);
    const std::vector<int> flatRows = flat.mapRows(0, flatCount, d, 3188);
    EXPECT_EQ(3187, flatRows.back());
    for (size_t i = 1; i < flatRows.size(); ++i) {
        ASSERT_LE(flatRows[i - 1], flatRows[i]) << i;
    }
}

TEST(BIFGeometryMaps, clampedToStorageExtent)
{
    const Aoi aoi = makeAoi(1, 1);
    const AoiGeometry geometry(aoi, estimateOverlaps(aoi, std::nullopt, std::nullopt));
    const std::vector<int> map = geometry.mapColumns(1020, 4, 1, 1022);
    EXPECT_EQ(std::vector<int>({1020, 1021, 1021, 1021}), map);
}

TEST(BIFGeometryInvalid, rejectsBadInput)
{
    const Aoi good = makeAoi(2, 2);
    const BoundaryOverlaps overlaps = estimateOverlaps(good, std::nullopt, std::nullopt);

    Aoi noCols = good;
    noCols.cols = 0;
    EXPECT_THROW(estimateOverlaps(noCols, std::nullopt, std::nullopt), RuntimeError);
    EXPECT_THROW(AoiGeometry(noCols, overlaps), RuntimeError);
    Aoi noTile = good;
    noTile.tileHeight = 0;
    EXPECT_THROW(AoiGeometry(noTile, overlaps), RuntimeError);
    EXPECT_THROW(AoiGeometry(good, BoundaryOverlaps{{0, 0}, {0}}), RuntimeError);
    EXPECT_THROW(AoiGeometry(good, BoundaryOverlaps{{1024}, {0}}), RuntimeError);

    const AoiGeometry geometry(good, overlaps);
    EXPECT_THROW(geometry.mapColumns(0, 1, 0, 100), RuntimeError);
    EXPECT_THROW(geometry.mapColumns(0, 1, 1, 0), RuntimeError);
    EXPECT_THROW(geometry.mapColumns(-1, 1, 1, 100), RuntimeError);
    EXPECT_THROW(geometry.mapRows(0, -1, 1, 100), RuntimeError);
    EXPECT_TRUE(geometry.mapRows(5, 0, 1, 100).empty());
}
