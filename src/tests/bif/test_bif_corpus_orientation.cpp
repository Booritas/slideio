// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// Gate for the stitching geometry (design §8): checks against real pixels that
// snakePosition() puts tiles the right way up. For each sampled joint, the two tiles it
// names are located in storage under each of four candidate orientations, and the
// overlap that best aligns their facing edges is searched for. Only the right
// orientation pairs tiles that really are neighbours, so only it reproduces the
// joint's recorded overlap.
//
// Overlaps belong to column and row boundaries (design §2.3), not to joints, and that
// limits what one axis can prove. Flipping the rows moves a horizontal joint to another
// row but keeps its column boundary: the tiles it then names are still true neighbours
// with the same overlap. So horizontal joints decide only whether columns are mirrored,
// and vertical joints only whether rows are flipped. An orientation is confirmed when it
// agrees on both axes; every other one must fail on at least one of them.
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <map>
#include <opencv2/imgproc.hpp>
#include "slideio/drivers/bif/bifxml.hpp"
#include "slideio/drivers/bif/bifdescription.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include "tests/testlib/testtools.hpp"

using namespace slideio;
using namespace slideio::bif;

namespace
{
    constexpr int StripWidth = 32;
    constexpr int MaxShift = 6;
    constexpr int Tolerance = 3;
    constexpr double MinMedianDistance = 8.;
    constexpr int MaxJointsPerAxis = 20;
    constexpr int MaxJointsPerBoundary = 2;
    constexpr int MinJointsPerAxis = 10;
    constexpr double MinStripStdDev = 4.;

    enum Orientation { AsDescribed, RowsFlipped, ColumnsMirrored, Both, OrientationCount };
    const char* orientationNames[OrientationCount] = {
        "as described", "rows flipped", "columns mirrored", "both" };

    cv::Point orient(cv::Point p, Orientation o, int cols, int rows)
    {
        if (o == RowsFlipped || o == Both) {
            p.y = rows - 1 - p.y;
        }
        if (o == ColumnsMirrored || o == Both) {
            p.x = cols - 1 - p.x;
        }
        return p;
    }

    class TileSource
    {
    public:
        explicit TileSource(const std::string& path)
        {
            m_tiff.openTiffFile(path);
            std::vector<TiffDirectory> dirs;
            TiffTools::scanFile(m_tiff.getHandle(), dirs);
            for (const auto& dir : dirs) {
                const auto level = parseLevelDescription(dir.description);
                if (level && level->level == 0) {
                    m_dir = dir;
                    m_found = true;
                    break;
                }
            }
        }
        bool found() const { return m_found; }
        const TiffDirectory& dir() const { return m_dir; }
        std::string packet() { return TiffTools::readXmlPacket(m_tiff.getHandle(), m_dir); }

        // Green channel of the storage tile at (col,row) of the level-0 tile grid.
        const cv::Mat& tile(cv::Point storage)
        {
            const int across = (m_dir.width + m_dir.tileWidth - 1) / m_dir.tileWidth;
            const int index = storage.y * across + storage.x;
            auto it = m_cache.find(index);
            if (it == m_cache.end()) {
                cv::Mat raster;
                m_tiff.readTile(m_dir, index, { 1 }, raster);
                it = m_cache.emplace(index, raster).first;
            }
            return it->second;
        }

    private:
        TIFFKeeper m_tiff;
        TiffDirectory m_dir;
        bool m_found = false;
        std::map<int, cv::Mat> m_cache;
    };

    struct Match
    {
        bool textured = false;
        int overlap = -1;
        double score = 0;
    };

    // `first` is the left (or upper) tile, `second` the right (or lower) one. The leading
    // strip of `second`, taken from the middle half of its facing edge, is searched for in
    // the last quarter of `first`, with a cross-axis slack of MaxShift pixels. The search
    // covers overlaps StripWidth..quarter, which holds every overlap the corpus records.
    Match findOverlap(const cv::Mat& first, const cv::Mat& second, bool horizontal)
    {
        cv::Mat a = horizontal ? first : first.t();
        cv::Mat b = horizontal ? second : second.t();
        const int along = a.cols;            // the axis the tiles are joined on
        const int across = a.rows;
        const int quarter = along / 4;
        const int top = across / 4;
        const int bottom = across - across / 4;
        const cv::Mat templ = b(cv::Range(top + MaxShift, bottom - MaxShift), cv::Range(0, StripWidth));
        const cv::Mat search = a(cv::Range(top, bottom), cv::Range(along - quarter, along));
        Match match;
        cv::Scalar mean, stddevT, stddevS;
        cv::meanStdDev(templ, mean, stddevT);
        cv::meanStdDev(search, mean, stddevS);
        match.textured = stddevT[0] >= MinStripStdDev && stddevS[0] >= MinStripStdDev;
        if (!match.textured) {
            return match;
        }
        cv::Mat ft, fs, result;
        templ.convertTo(ft, CV_32F);
        search.convertTo(fs, CV_32F);
        cv::matchTemplate(fs, ft, result, cv::TM_CCOEFF_NORMED);
        cv::Point best;
        cv::minMaxLoc(result, nullptr, &match.score, nullptr, &best);
        // search column x lines up with column 0 of `second`
        match.overlap = quarter - best.x;
        return match;
    }

    double median(std::vector<double> values)
    {
        std::sort(values.begin(), values.end());
        return values.empty() ? 0. : values[values.size() / 2];
    }

    struct AxisCounts
    {
        int evaluated = 0;
        std::array<int, OrientationCount> agree{};
    };

    AxisCounts checkAxis(TileSource& source, const Aoi& aoi, JointAxis axis, const std::string& label)
    {
        const bool horizontal = axis == JointAxis::Horizontal;
        auto overlapOf = [horizontal](const Joint& j) { return horizontal ? j.overlapX : j.overlapY; };
        std::vector<double> overlaps;
        for (const auto& joint : aoi.joints) {
            if (joint.axis == axis) {
                overlaps.push_back(overlapOf(joint));
            }
        }
        const double axisMedian = median(overlaps);
        std::vector<Joint> candidates;
        for (const auto& joint : aoi.joints) {
            if (joint.axis == axis && std::abs(overlapOf(joint) - axisMedian) >= MinMedianDistance) {
                candidates.push_back(joint);
            }
        }
        // the joints that differ most from the median discriminate best
        std::stable_sort(candidates.begin(), candidates.end(), [&](const Joint& l, const Joint& r) {
            return std::abs(overlapOf(l) - axisMedian) > std::abs(overlapOf(r) - axisMedian);
        });
        std::cout << label << (horizontal ? " horizontal" : " vertical") << ": median overlap "
            << axisMedian << ", " << candidates.size() << " candidate joints" << std::endl;

        const cv::Point base(aoi.originX / aoi.tileWidth, aoi.originY / aoi.tileHeight);
        AxisCounts counts;
        // a few joints from each boundary, so that the sample spans several of them
        std::map<int, int> perBoundary;
        for (const auto& joint : candidates) {
            if (counts.evaluated >= MaxJointsPerAxis) {
                break;
            }
            const cv::Point described = snakePosition(joint.tile1, aoi.cols, aoi.rows);
            int& boundaryCount = perBoundary[horizontal ? described.x : described.y];
            if (boundaryCount >= MaxJointsPerBoundary) {
                continue;
            }
            std::array<Match, OrientationCount> matches;
            bool textured = true;
            for (int o = 0; o < OrientationCount && textured; ++o) {
                const auto orientation = static_cast<Orientation>(o);
                const cv::Point p1 = orient(snakePosition(joint.tile1, aoi.cols, aoi.rows), orientation,
                    aoi.cols, aoi.rows);
                const cv::Point p2 = orient(snakePosition(joint.tile2, aoi.cols, aoi.rows), orientation,
                    aoi.cols, aoi.rows);
                // which tile is left/above follows from the positions, not from the label
                const bool firstIsTile1 = horizontal ? p1.x < p2.x : p1.y < p2.y;
                const cv::Mat& t1 = source.tile(base + p1);
                const cv::Mat& t2 = source.tile(base + p2);
                matches[o] = firstIsTile1 ? findOverlap(t1, t2, horizontal) : findOverlap(t2, t1, horizontal);
                textured = matches[o].textured;
            }
            // a joint on blank glass says nothing about any orientation
            if (!textured) {
                continue;
            }
            ++counts.evaluated;
            ++boundaryCount;
            const double expected = overlapOf(joint);
            std::cout << "  joint " << joint.tile1 << "->" << joint.tile2 << " overlap " << expected << ":";
            for (int o = 0; o < OrientationCount; ++o) {
                const bool agrees = std::abs(matches[o].overlap - expected) <= Tolerance;
                counts.agree[o] += agrees ? 1 : 0;
                std::cout << " [" << orientationNames[o] << " k=" << matches[o].overlap
                    << " ncc=" << cv::format("%.2f", matches[o].score) << (agrees ? " *" : "") << "]";
            }
            std::cout << std::endl;
        }
        std::cout << label << (horizontal ? " horizontal" : " vertical") << ": " << counts.evaluated
            << " joints evaluated;";
        for (int o = 0; o < OrientationCount; ++o) {
            std::cout << " " << orientationNames[o] << " " << counts.agree[o] << "/" << counts.evaluated << ";";
        }
        std::cout << std::endl;
        return counts;
    }

    void checkAoi(TileSource& source, const Aoi& aoi, const std::string& label)
    {
        ASSERT_TRUE(aoi.scanned) << label;
        ASSERT_EQ(source.dir().tileWidth, aoi.tileWidth) << label;
        ASSERT_EQ(source.dir().tileHeight, aoi.tileHeight) << label;
        ASSERT_EQ(0, aoi.originX % aoi.tileWidth) << label;
        ASSERT_EQ(0, aoi.originY % aoi.tileHeight) << label;
        std::array<AxisCounts, 2> axes;
        for (JointAxis axis : { JointAxis::Horizontal, JointAxis::Vertical }) {
            const bool horizontal = axis == JointAxis::Horizontal;
            const AxisCounts counts = checkAxis(source, aoi, axis, label);
            const std::string what = label + (horizontal ? " horizontal" : " vertical");
            ASSERT_GE(counts.evaluated, MinJointsPerAxis) << what;
            ::testing::Test::RecordProperty(what + " evaluated", counts.evaluated);
            for (int o = 0; o < OrientationCount; ++o) {
                ::testing::Test::RecordProperty(what + " " + orientationNames[o], counts.agree[o]);
            }
            axes[horizontal ? 0 : 1] = counts;
        }
        auto fraction = [&](int axis, int o) {
            return static_cast<double>(axes[axis].agree[o]) / axes[axis].evaluated;
        };
        EXPECT_GE(fraction(0, AsDescribed), 0.8) << label << ": as described, horizontal joints";
        EXPECT_GE(fraction(1, AsDescribed), 0.8) << label << ": as described, vertical joints";
        for (int o = AsDescribed + 1; o < OrientationCount; ++o) {
            EXPECT_LT(std::min(fraction(0, o), fraction(1, o)), 0.5)
                << label << ": " << orientationNames[o] << " agrees on both axes";
        }
    }
}

TEST(BIFCorpusOrientation, SnakeMatchesPixels)
{
    const std::string os1 = TestTools::getTestImagePath("bif/openslide", "OS-1.bif");
    const std::string os2 = TestTools::getTestImagePath("bif/openslide", "OS-2.bif");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(os1);
    SLIDEIO_SKIP_IF_IMAGE_MISSING(os2);

    {
        TileSource source(os1);
        ASSERT_TRUE(source.found());
        const StitchInfo stitch = parseStitchXml(source.packet());
        ASSERT_EQ(1u, stitch.aois.size());
        checkAoi(source, stitch.aois[0], "OS-1 AOI0");
    }
    {
        TileSource source(os2);
        ASSERT_TRUE(source.found());
        const StitchInfo stitch = parseStitchXml(source.packet());
        ASSERT_GE(stitch.aois.size(), 3u);
        checkAoi(source, stitch.aois[2], "OS-2 AOI2");
    }
}
