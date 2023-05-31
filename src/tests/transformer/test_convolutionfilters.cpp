#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include "slideio/converter/converter.hpp"
#include "slideio/slideio/slideio.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/base/exceptions.hpp"
#include <boost/filesystem.hpp>
#include <opencv2/imgproc.hpp>
#include "slideio/transformer/colortransformation.hpp"
#include "slideio/transformer/convolutionfilter.hpp"
#include "slideio/transformer/convolutionfilterscene.hpp"
#include "slideio/transformer/transformer.hpp"
#include "tests/testlib/testscene.hpp"

using namespace slideio;

TEST(ConvolutionFiltes, getBlockExtensionForGaussianBlur)
{
    std::shared_ptr<TestScene> originScene(new TestScene);
    GaussianBlurFilter filter;
    ConvolutionFilterScene scene(originScene, filter);
    filter.setKernelSizeX(3);
    filter.setKernelSizeY(3);
    int extension = scene.getBlockExtensionForGaussianBlur(filter);
    EXPECT_EQ(2, extension);
    filter.setKernelSizeX(5);
    filter.setKernelSizeY(3);
    extension = scene.getBlockExtensionForGaussianBlur(filter);
    EXPECT_EQ(3, extension);
    filter.setKernelSizeX(0);
    filter.setKernelSizeY(0);
    filter.setSigmaX(1.0);
    filter.setSigmaY(1.0);
    extension = scene.getBlockExtensionForGaussianBlur(filter);
    EXPECT_EQ(3, extension);
    filter.setSigmaX(2.0);
    filter.setSigmaY(1.0);
    extension = scene.getBlockExtensionForGaussianBlur(filter);
    EXPECT_EQ(5, extension);
    filter.setSigmaX(1.5);
    filter.setSigmaY(1.0);
    extension = scene.getBlockExtensionForGaussianBlur(filter);
    EXPECT_EQ(4, extension);
}

TEST(ConvolutionFiltes, extendBlockRect)
{
    std::shared_ptr<TestScene> originScene(new TestScene);
    originScene->setRect(cv::Rect(0, 0, 1000, 1000));
    GaussianBlurFilter filter;
    filter.setKernelSizeX(3);
    filter.setKernelSizeY(3);
    cv::Rect rect(10, 20, 100, 200);
    ConvolutionFilterScene scene(originScene, filter);
    cv::Rect extendedRect = scene.extendBlockRect(rect);
    EXPECT_EQ(8, extendedRect.x);
    EXPECT_EQ(18, extendedRect.y);
    EXPECT_EQ(104, extendedRect.width);
    EXPECT_EQ(204, extendedRect.height);
    rect = cv::Rect(0, 0, 1000, 1000);
    extendedRect = scene.extendBlockRect(rect);
    EXPECT_EQ(0, extendedRect.x);
    EXPECT_EQ(0, extendedRect.y);
    EXPECT_EQ(1000, extendedRect.width);
    EXPECT_EQ(1000, extendedRect.height);
    rect = cv::Rect(1, 2, 999, 998);
    extendedRect = scene.extendBlockRect(rect);
    EXPECT_EQ(0, extendedRect.x);
    EXPECT_EQ(0, extendedRect.y);
    EXPECT_EQ(1000, extendedRect.width);
    EXPECT_EQ(1000, extendedRect.height);
    rect = cv::Rect(100, 2, 100, 200);
    extendedRect = scene.extendBlockRect(rect);
    EXPECT_EQ(98, extendedRect.x);
    EXPECT_EQ(0, extendedRect.y);
    EXPECT_EQ(104, extendedRect.width);
    EXPECT_EQ(204, extendedRect.height);
    rect = cv::Rect(100, 1, 100, 200);
    extendedRect = scene.extendBlockRect(rect);
    EXPECT_EQ(98, extendedRect.x);
    EXPECT_EQ(0, extendedRect.y);
    EXPECT_EQ(104, extendedRect.width);
    EXPECT_EQ(203, extendedRect.height);
}