// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include "slideio/core/exceptions.hpp"
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
    expectMatchesCvResize(src, cv::Size(150, 100), limits, 1);  // 50-wide tiles split every row
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

namespace
{
    cv::Mat randomMat(int rows, int cols, int type, uint64_t seed)
    {
        cv::Mat m(rows, cols, type);
        cv::RNG rng(seed);
        const int depth = CV_MAT_DEPTH(type);
        if (depth == CV_32F) {
            rng.fill(m, cv::RNG::UNIFORM, 0., 1000.);
        }
        else if (depth == CV_8U) {
            rng.fill(m, cv::RNG::UNIFORM, 0, 256);
        }
        else if (depth == CV_16U) {
            rng.fill(m, cv::RNG::UNIFORM, 0, 65536);
        }
        else {
            rng.fill(m, cv::RNG::UNIFORM, -32768, 32768);
        }
        return m;
    }
}

// The integer-ratio path against cv::resize for every channel count and the depths
// OpenCV treats differently (SIMD 2 x 2 rounding vs. float scale).
TEST(TiledAreaDownscale, IntegerRatioIsExactForAllChannelCounts)
{
    const int depths[] = {CV_8U, CV_16U, CV_16S, CV_32F};
    for (int ratio : {2, 3}) {
        for (int depth : depths) {
            for (int cn = 1; cn <= 5; ++cn) {
                const cv::Mat src = randomMat(60, 90, CV_MAKETYPE(depth, cn), 1000 + depth * 10 + cn);
                ResampleLimits limits;
                limits.maxResampleSourcePixels = int64_t(90) * 60;
                SCOPED_TRACE(testing::Message() << "ratio " << ratio << " depth " << depth << " cn " << cn);
                const double tol = depth == CV_32F ? 1e-3 : 0;
                expectMatchesCvResize(src, cv::Size(90 / ratio, 60 / ratio), limits, tol);
            }
        }
    }
}

TEST(TiledAreaDownscale, RejectsTileOfWrongShape)
{
    const cv::Mat src = randomMat(40, 40, CV_16UC2, 5);
    ResampleLimits limits;
    limits.maxResampleSourcePixels = 100;
    const cv::Rect rect(0, 0, 40, 40);
    cv::Mat out;
    // Wrong channel count.
    EXPECT_THROW(tiledAreaDownscale(rect, cv::Size(10, 10), CV_16U, 2, limits,
        [&](const cv::Rect& r, cv::Mat& tile) {
            cv::Mat one;
            cv::extractChannel(src(r), one, 0);
            tile = one;
        }, out), slideio::RuntimeError);
    // Wrong size.
    EXPECT_THROW(tiledAreaDownscale(rect, cv::Size(10, 10), CV_16U, 2, limits,
        [&](const cv::Rect& r, cv::Mat& tile) {
            src(cv::Rect(r.x, r.y, r.width, 1)).copyTo(tile);
        }, out), slideio::RuntimeError);
}
