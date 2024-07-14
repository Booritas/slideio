#include <gtest/gtest.h>
#include <opencv2/core.hpp>

#include "slideio/base/exceptions.hpp"
#include "slideio/core/tools/blocktiler.hpp"
#include "slideio/core/tools/recttiler.hpp"
#include "slideio/core/tools/recttilevisitor.hpp"

TEST(RectTiler, ApplyVisitor) {
    const int value = 100;
    cv::Size rectSize(543, 483);
    cv::Size tileSize(9, 10);
    cv::Mat image(rectSize, CV_8UC1);
    const int area = rectSize.width * rectSize.height;
    int processedArea = 0;

    slideio::RectTiler tiler(rectSize, tileSize);
    tiler.apply([&image, &processedArea, value](const cv::Rect& rect) {
        cv::Mat block = image(rect);
        block.setTo(cv::Scalar::all(value));
        processedArea += rect.width * rect.height;
    });
    EXPECT_EQ(processedArea, area);
    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(image, &minVal, &maxVal, &minLoc, &maxLoc);
    EXPECT_EQ(int(minVal), int(maxVal));
    EXPECT_EQ(minVal, value);
}

TEST(RectTiler, InvalidTileSize) {
    cv::Size rectSize(100, 100); // Create a block of size 100x100 with 3 channels
    cv::Size tileSize(0, 0); // Set an invalid tile size
    EXPECT_THROW(slideio::RectTiler tiler(rectSize, tileSize), slideio::RuntimeError);
}

TEST(RectTiler, EmptyBlock) {
    cv::Size rectSize; // Create an empty block
    cv::Size tileSize(10, 10); // Set the tile size to 10x10
    EXPECT_THROW(slideio::RectTiler tiler(rectSize, tileSize), slideio::RuntimeError);
}

TEST(RectTiler, NullVisitor) {
    cv::Size rectSize(100, 100);
    cv::Size tileSize(10, 10); // Set the tile size to 10x10
    slideio::RectTiler tiler(rectSize, tileSize);
    EXPECT_THROW(tiler.apply(nullptr), slideio::RuntimeError);
}
