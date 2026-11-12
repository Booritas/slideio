// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include "slideio/core/tools/tiledareadownscale.hpp"

using namespace slideio;

namespace
{
    void expectMatchesCvResize(const cv::Mat& src, const cv::Size& size, const ResampleLimits& limits, double tolerance = 0)
    {
        const TileReader read = [&](const cv::Rect& rect, cv::Mat& tile) {
            src(rect).copyTo(tile);
        };
        cv::Mat out;
        tiledAreaDownscale(cv::Rect(0, 0, src.cols, src.rows), size, src.depth(), src.channels(), limits, read, out);
        cv::Mat expected;
        cv::resize(src, expected, size, 0, 0, cv::INTER_AREA);
        ASSERT_EQ(out.size(), expected.size());
        ASSERT_EQ(out.type(), expected.type());
        EXPECT_LE(cv::norm(out, expected, cv::NORM_INF), tolerance);
    }
}

TEST(TiledAreaDownscale, MatchesCvResize)
{
    cv::Mat src(200, 300, CV_16UC2);
    cv::RNG rng(12345);
    rng.fill(src, cv::RNG::UNIFORM, 0, 65536);
    ResampleLimits limits;
    limits.maxResampleSourcePixels = 50;
    limits.stats = std::make_shared<ResampleStats>();
    // Tiles narrower than the rectangle sum non-integer ratios in a different float
    // order than OpenCV does: within one step (the contract LIF always had).
    expectMatchesCvResize(src, cv::Size(77, 31), limits, 1);
    expectMatchesCvResize(src, cv::Size(150, 100), limits, 1);
    EXPECT_LE(limits.stats->peakSourcePixels.load(), 50);
    EXPECT_GT(limits.stats->peakSourcePixels.load(), 0);
}

TEST(TiledAreaDownscale, NeedsTiled)
{
    ResampleLimits limits;
    limits.maxResampleSourcePixels = 16;
    EXPECT_TRUE(needsTiledDownscale(cv::Size(1000, 1000), cv::Size(10, 10), limits));
    EXPECT_FALSE(needsTiledDownscale(cv::Size(4, 4), cv::Size(2, 2), limits));
    EXPECT_FALSE(needsTiledDownscale(cv::Size(1000, 1000), cv::Size(2000, 10), limits));
    EXPECT_FALSE(needsTiledDownscale(cv::Size(1000, 1000), cv::Size(1000, 1000), limits));
}

TEST(TiledAreaDownscale, FullWidthTilesAreBitExact)
{
    cv::Mat src(200, 300, CV_16UC2);
    cv::RNG rng(777);
    rng.fill(src, cv::RNG::UNIFORM, 0, 65536);
    ResampleLimits limits;
    limits.maxResampleSourcePixels = 300 * 7;   // a tile is as wide as the rectangle
    expectMatchesCvResize(src, cv::Size(77, 31), limits);
}
