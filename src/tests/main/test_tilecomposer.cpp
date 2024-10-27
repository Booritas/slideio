#include <gtest/gtest.h>
#include "slideio-opencv/core.hpp"

#include "tests/testlib/testtiler.hpp"

TEST(TileComposer, composeRect)
{
    const int tileWidth(100), tileHeight(200), tilesX(6), tilesY(3);
    cv::Scalar white(255, 255, 255), black(0, 0, 0);
    TestTiler testTiler(tileWidth, tileHeight, tilesX, tilesY, black, white);
    cv::Mat image;

    const int imageWidth = tilesX * tileWidth;
    const int imageHeight = tilesY * tileHeight;
    const int dX = tileWidth / 2;
    const int dY = tileHeight / 2;

    const std::vector<int> channelIndices;
    const cv::Rect imageRect = { dX, dY, imageWidth - dX * 2, imageHeight - dY * 2 };
    const cv::Size blockSize = { imageRect.width / 2, imageRect.height / 4 };
    slideio::TileComposer::composeRect(&testTiler, channelIndices, imageRect, blockSize, image, nullptr);

    cv::Rect subWhiteImageRect(dX / 2, dY / 4, tileWidth / 2, tileHeight / 4);
    cv::Rect subBlackImageRect(dX / 2 + tileWidth / 2, dY / 4, tileWidth / 2, tileHeight / 4);

    cv::Mat blackImage = image(subBlackImageRect);
    cv::Mat whiteImage = image(subWhiteImageRect);

    cv::Scalar blackMean, blackStddev, whiteMean, whiteStddev;
    meanStdDev(blackImage, blackMean, blackStddev);
    meanStdDev(whiteImage, whiteMean, whiteStddev);
    EXPECT_TRUE(whiteMean==white);
    EXPECT_TRUE(blackMean==black);
    EXPECT_TRUE(whiteStddev==cv::Scalar(0, 0, 0));
    EXPECT_TRUE(blackStddev==cv::Scalar(0, 0, 0));
    
}