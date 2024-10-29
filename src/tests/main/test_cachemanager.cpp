
#include <gtest/gtest.h>
#include "slideio/base/exceptions.hpp"
#include "slideio/core/tools/cachemanager.hpp"
#include "tests/testlib/testtools.hpp"

using namespace slideio;


TEST(CacheManagerTest, addAndGetTiles)
{
    CacheManager cacheManager;

    const cv::Rect rect1(10, 20, 200, 300);
    const cv::Mat tile1(rect1.size(), CV_8UC3, cv::Scalar(255, 0, 0));
    cacheManager.addTile(0, rect1.tl(), tile1);

    EXPECT_EQ(cacheManager.getTileCount(0), 1);
    EXPECT_EQ(cacheManager.getTileCount(1), 0);

    const cv::Rect rect2(14, 22, 210, 330);
    const cv::Mat tile2(rect2.size(), CV_8UC3, cv::Scalar(255, 255, 0));
    cacheManager.addTile(0, rect2.tl(), tile2);
    EXPECT_EQ(cacheManager.getTileCount(0), 2);

    cv::Mat tl1 = cacheManager.getTile(0, 0);
    cv::Rect r1 = cacheManager.getTileRect(0, 0);
    EXPECT_EQ(r1, rect1);
    ASSERT_EQ(tile1.rows, tl1.rows);
    ASSERT_EQ(tile1.cols, tl1.cols);
    ASSERT_EQ(tile1.type(), tl1.type());
    ASSERT_TRUE(TestTools::isZeroMat(tile1 != tl1));

    cv::Mat tl2 = cacheManager.getTile(0, 1);
    cv::Rect r2 = cacheManager.getTileRect(0, 1);
    EXPECT_EQ(r2, rect2);
    ASSERT_EQ(tile2.rows, tl2.rows);
    ASSERT_EQ(tile2.cols, tl2.cols);
    ASSERT_EQ(tile2.type(), tl2.type());
    ASSERT_TRUE(TestTools::isZeroMat(tile2 != tl2));

    EXPECT_THROW(cacheManager.getTile(3, 3), slideio::RuntimeError);
    EXPECT_THROW(cacheManager.getTile(0, 3), slideio::RuntimeError);
}