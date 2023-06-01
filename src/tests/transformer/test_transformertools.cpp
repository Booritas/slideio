#include <gtest/gtest.h>
#include "slideio/transformer/transformertools.hpp"

using namespace slideio;

static bool areRectsEqual(const cv::Rect& rect1, const cv::Rect& rect2)
{
    return rect1.x == rect2.x &&
        rect1.y == rect2.y &&
        rect1.width == rect2.width &&
        rect1.height == rect2.height;
}

static bool areSizesEqual(const cv::Size& size1, const cv::Size& size2)
{
    return size1.width == size2.width &&
        size1.height == size2.height;
}

static bool arePointsEqual(const cv::Point& pnt1, const cv::Point& pnt2)
{
    return pnt1.x == pnt2.x &&
        pnt1.y == pnt2.y;
}

TEST(TransformerTools, computeInflatedRectParams_blockInMiddle)
{
    const cv::Size sceneSize(1000, 2000);
    const cv::Rect blockRect(100, 200, 200, 100);
    const cv::Size blockSize(200, 100);
    cv::Rect inflatedBlockRect;
    cv::Size inflatedSize;
    cv::Point blockPositionInInflatedRect;
    const int inflationValue = 3;

    TransformerTools::computeInflatedRectParams(sceneSize, blockRect,  inflationValue, 
        blockSize, inflatedBlockRect, inflatedSize, blockPositionInInflatedRect);

    const cv::Rect expectedInflatedBlockRect(
        blockRect.x-inflationValue,
        blockRect.y - inflationValue,
        blockRect.width+2*inflationValue,
        blockRect.height+2*inflationValue
    );
    const cv::Size expectedInflatedSize(expectedInflatedBlockRect.size());
    const cv::Point expectedBlockPositionInInflatedRect(inflationValue, inflationValue);
    EXPECT_TRUE(areRectsEqual(expectedInflatedBlockRect,inflatedBlockRect));
    EXPECT_TRUE(areSizesEqual(expectedInflatedSize, inflatedSize));
    EXPECT_TRUE(arePointsEqual(expectedBlockPositionInInflatedRect, blockPositionInInflatedRect));
}

TEST(TransformerTools, computeInflatedRectParams_scaledBlockInMiddle)
{
    const cv::Size sceneSize(1000, 2000);
    const cv::Rect blockRect(100, 200, 200, 100);
    const cv::Size blockSize(100, 50);
    cv::Rect inflatedBlockRect;
    cv::Size inflatedSize;
    cv::Point blockPositionInInflatedRect;
    const int inflationValue = 4;

    TransformerTools::computeInflatedRectParams(sceneSize, blockRect, inflationValue,
        blockSize, inflatedBlockRect, inflatedSize, blockPositionInInflatedRect);

    const cv::Rect expectedInflatedBlockRect(
        blockRect.x - inflationValue*2,
        blockRect.y - inflationValue*2,
        blockRect.width + 4 * inflationValue,
        blockRect.height + 4 * inflationValue
    );
    const cv::Size expectedInflatedSize(expectedInflatedBlockRect.width/2, expectedInflatedBlockRect.height/2);
    const cv::Point expectedBlockPositionInInflatedRect(inflationValue, inflationValue);
    EXPECT_TRUE(areRectsEqual(expectedInflatedBlockRect, inflatedBlockRect));
    EXPECT_TRUE(areSizesEqual(expectedInflatedSize, inflatedSize));
    EXPECT_TRUE(arePointsEqual(expectedBlockPositionInInflatedRect, blockPositionInInflatedRect));
}

TEST(TransformerTools, computeInflatedRectParams_wholeSceneBlock)
{
    const cv::Size sceneSize(1000, 2000);
    const cv::Rect blockRect(0, 0, 1000, 2000);
    const cv::Size blockSize(1000, 2000);
    cv::Rect inflatedBlockRect;
    cv::Size inflatedSize;
    cv::Point blockPositionInInflatedRect;
    const int inflationValue = 3;

    TransformerTools::computeInflatedRectParams(sceneSize, blockRect, inflationValue,
        blockSize, inflatedBlockRect, inflatedSize, blockPositionInInflatedRect);

    const cv::Rect expectedInflatedBlockRect(blockRect);
    const cv::Size expectedInflatedSize(expectedInflatedBlockRect.size());
    const cv::Point expectedBlockPositionInInflatedRect(0, 0);
    EXPECT_TRUE(areRectsEqual(expectedInflatedBlockRect, inflatedBlockRect));
    EXPECT_TRUE(areSizesEqual(expectedInflatedSize, inflatedSize));
    EXPECT_TRUE(arePointsEqual(expectedBlockPositionInInflatedRect, blockPositionInInflatedRect));
}

TEST(TransformerTools, computeInflatedRectParams_scaledWholeSceneBlock)
{
    const cv::Size sceneSize(1000, 2000);
    const cv::Rect blockRect(0, 0, 1000, 2000);
    const cv::Size blockSize(100, 200);
    cv::Rect inflatedBlockRect;
    cv::Size inflatedSize;
    cv::Point blockPositionInInflatedRect;
    const int inflationValue = 3;

    TransformerTools::computeInflatedRectParams(sceneSize, blockRect, inflationValue,
        blockSize, inflatedBlockRect, inflatedSize, blockPositionInInflatedRect);

    const cv::Rect expectedInflatedBlockRect(blockRect);
    const cv::Size expectedInflatedSize(blockSize);
    const cv::Point expectedBlockPositionInInflatedRect(0, 0);
    EXPECT_TRUE(areRectsEqual(expectedInflatedBlockRect, inflatedBlockRect));
    EXPECT_TRUE(areSizesEqual(expectedInflatedSize, inflatedSize));
    EXPECT_TRUE(arePointsEqual(expectedBlockPositionInInflatedRect, blockPositionInInflatedRect));
}

TEST(TransformerTools, computeInflatedRectParams_scaledLeftSceneBlock)
{
    const cv::Size sceneSize(1000, 2000);
    const cv::Rect blockRect(0, 0, 200, 400);
    const cv::Size blockSize(100, 200);
    cv::Rect inflatedBlockRect;
    cv::Size inflatedSize;
    cv::Point blockPositionInInflatedRect;
    const int inflationValue = 4;

    TransformerTools::computeInflatedRectParams(sceneSize, blockRect, inflationValue,
        blockSize, inflatedBlockRect, inflatedSize, blockPositionInInflatedRect);

    const cv::Rect expectedInflatedBlockRect(
        0, 0,
        blockRect.width + 2 * inflationValue,
        blockRect.height + 2 * inflationValue
    );
    const cv::Size expectedInflatedSize(expectedInflatedBlockRect.width / 2, expectedInflatedBlockRect.height / 2);
    const cv::Point expectedBlockPositionInInflatedRect(0, 0);
    EXPECT_TRUE(areRectsEqual(expectedInflatedBlockRect, inflatedBlockRect));
    EXPECT_TRUE(areSizesEqual(expectedInflatedSize, inflatedSize));
    EXPECT_TRUE(arePointsEqual(expectedBlockPositionInInflatedRect, blockPositionInInflatedRect));
}


TEST(TransformerTools, computeInflatedRectParams_scaledRightSceneBlock)
{
    const cv::Size sceneSize(1000, 2000);
    const cv::Rect blockRect(800, 1600, 200, 400);
    const cv::Size blockSize(100, 200);
    cv::Rect inflatedBlockRect;
    cv::Size inflatedSize;
    cv::Point blockPositionInInflatedRect;
    const int inflationValue = 4;

    TransformerTools::computeInflatedRectParams(sceneSize, blockRect, inflationValue,
        blockSize, inflatedBlockRect, inflatedSize, blockPositionInInflatedRect);

    const cv::Rect expectedInflatedBlockRect(
        792, 1592,
        blockRect.width + 2 * inflationValue,
        blockRect.height + 2 * inflationValue
    );
    const cv::Size expectedInflatedSize(expectedInflatedBlockRect.width / 2, expectedInflatedBlockRect.height / 2);
    const cv::Point expectedBlockPositionInInflatedRect(inflationValue, inflationValue);
    EXPECT_TRUE(areRectsEqual(expectedInflatedBlockRect, inflatedBlockRect));
    EXPECT_TRUE(areSizesEqual(expectedInflatedSize, inflatedSize));
    EXPECT_TRUE(arePointsEqual(expectedBlockPositionInInflatedRect, blockPositionInInflatedRect));
}

TEST(TransformerTools, getBlockExtensionForGaussianBlur)
{
    GaussianBlurFilter filter;
    filter.setKernelSizeX(3);
    filter.setKernelSizeY(3);
    int extension = TransformerTools::getBlockExtensionForGaussianBlur(filter);
    EXPECT_EQ(2, extension);
    filter.setKernelSizeX(5);
    filter.setKernelSizeY(3);
    extension = TransformerTools::getBlockExtensionForGaussianBlur(filter);
    EXPECT_EQ(3, extension);
    filter.setKernelSizeX(0);
    filter.setKernelSizeY(0);
    filter.setSigmaX(1.0);
    filter.setSigmaY(1.0);
    extension = TransformerTools::getBlockExtensionForGaussianBlur(filter);
    EXPECT_EQ(3, extension);
    filter.setSigmaX(2.0);
    filter.setSigmaY(1.0);
    extension = TransformerTools::getBlockExtensionForGaussianBlur(filter);
    EXPECT_EQ(5, extension);
    filter.setSigmaX(1.5);
    filter.setSigmaY(1.0);
    extension = TransformerTools::getBlockExtensionForGaussianBlur(filter);
    EXPECT_EQ(4, extension);
}
