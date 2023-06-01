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

void testBlockRect(ConvolutionFilterScene& scene, const cv::Rect& sceneRect, int kernelSize)
{
    const int width = sceneRect.width;
    const int height = sceneRect.height;

    // block rect is inside scene rect
    int x = kernelSize * 3;
    int y = kernelSize * 3;
    int blockWidth = width - x*2;
    int blockHeight = height - y*2;
    int kernel2 = (kernelSize + 1) / 2;
    cv::Rect rect(x, y, blockWidth, blockHeight);
    cv::Rect extendedRect = scene.extendBlockRect(rect);
    EXPECT_EQ(x - kernel2, extendedRect.x);
    EXPECT_EQ(y - kernel2, extendedRect.y);
    EXPECT_EQ(blockWidth + kernel2*2, extendedRect.width);
    EXPECT_EQ(blockHeight + kernel2*2, extendedRect.height);

    // block rect is equal to scene rect
    rect = cv::Rect(0, 0, width, height);
    extendedRect = scene.extendBlockRect(rect);
    EXPECT_EQ(0, extendedRect.x);
    EXPECT_EQ(0, extendedRect.y);
    EXPECT_EQ(width, extendedRect.width);
    EXPECT_EQ(height, extendedRect.height);

    // block rect is on the top left border
    blockWidth = width/4;
    blockHeight = height/4;
    rect = cv::Rect(0, 0, blockWidth, blockHeight);
    extendedRect = scene.extendBlockRect(rect);
    EXPECT_EQ(0, extendedRect.x);
    EXPECT_EQ(0, extendedRect.y);
    EXPECT_EQ(blockWidth + kernel2, extendedRect.width);
    EXPECT_EQ(blockHeight + kernel2, extendedRect.height);

    // block is on the bottom right border
    x = width/6;
    y = height/6;
    blockWidth = width - x;
    blockHeight = height - y;
    rect = cv::Rect(x, y, blockWidth, blockHeight);
    extendedRect = scene.extendBlockRect(rect);
    EXPECT_EQ(x - kernel2, extendedRect.x);
    EXPECT_EQ(y - kernel2, extendedRect.y);
    EXPECT_EQ(blockWidth + kernel2, extendedRect.width);
    EXPECT_EQ(blockHeight + kernel2, extendedRect.height);

    // block is close to the top left border
    x = 1;
    y = 2;
    blockWidth = width/4;
    blockHeight = height/4;
    rect = cv::Rect(x, y, blockWidth, blockHeight);
    extendedRect = scene.extendBlockRect(rect);
    EXPECT_EQ(0, extendedRect.x);
    EXPECT_EQ(0, extendedRect.y);
    EXPECT_EQ(blockWidth + kernel2 + x, extendedRect.width);
    EXPECT_EQ(blockHeight + kernel2 + y, extendedRect.height);

    // block is close to the bottom right border
    const int dx = 1;
    const int dy = 2;
    blockWidth = width / 4;
    blockHeight = height / 4;
    x = width - blockWidth - dx;
    y = height - blockHeight - dy;
    rect = cv::Rect(x, y, blockWidth, blockHeight);
    extendedRect = scene.extendBlockRect(rect);
    EXPECT_EQ(x-kernel2, extendedRect.x);
    EXPECT_EQ(y-kernel2, extendedRect.y);
    EXPECT_EQ(blockWidth + kernel2 + dx, extendedRect.width);
    EXPECT_EQ(blockHeight + kernel2 + dy, extendedRect.height);
}

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
    cv::Rect sceneRect = cv::Rect(0, 0, 1000, 1000);
    originScene->setRect(sceneRect);
    {
        GaussianBlurFilter filter;
        filter.setKernelSizeX(3);
        filter.setKernelSizeY(3);
        ConvolutionFilterScene scene(originScene, filter);
        testBlockRect(scene, sceneRect, 3);
    }
    {
        MedianBlurFilter filter;
        filter.setKernelSize(5);
        ConvolutionFilterScene scene(originScene, filter);
        testBlockRect(scene, sceneRect, 5);
    }
    {
        SobelFilter filter;
        filter.setKernelSize(5);
        ConvolutionFilterScene scene(originScene, filter);
        testBlockRect(scene, sceneRect, 5);
    }
    {
        ScharrFilter filter;
        ConvolutionFilterScene scene(originScene, filter);
        testBlockRect(scene, sceneRect, 3);
    }
}

TEST(ConvolutionFiltes, applyTransformationGaussianBlur)
{
    std::string path = TestTools::getTestImagePath("gdal", "colors.png");
    std::shared_ptr<slideio::Slide> slide = openSlide(path, "AUTO");
    std::shared_ptr<slideio::Scene> originScene = slide->getScene(0);

    std::tuple<int, int, int, int> rect = originScene->getRect();
    const int width = std::get<2>(rect);
    const int height = std::get<3>(rect);
    const int bufferSize = width * height * 3;
    std::vector<unsigned char> buffer(bufferSize);
    originScene->readBlock(rect, buffer.data(), buffer.size());
    cv::Mat originImage(height, width, CV_8UC3, buffer.data());

    GaussianBlurFilter filter;
    const int kernelSize = 15;
    filter.setKernelSizeX(kernelSize);
    filter.setKernelSizeY(kernelSize);
    ConvolutionFilterScene scene(originScene->getCVScene(), filter);
    cv::Mat transformedImage;
    scene.appyTransformation(originImage, transformedImage);
    cv::Mat testImage;
    cv::GaussianBlur(originImage, testImage, cv::Size(kernelSize, kernelSize), 0);
    TestTools::compareRasters(testImage, transformedImage);

}

TEST(ConvolutionFiltes, applyTransformationMedianBlur)
{
    std::string path = TestTools::getTestImagePath("gdal", "colors.png");
    std::shared_ptr<slideio::Slide> slide = openSlide(path, "AUTO");
    std::shared_ptr<slideio::Scene> originScene = slide->getScene(0);

    std::tuple<int, int, int, int> rect = originScene->getRect();
    const int width = std::get<2>(rect);
    const int height = std::get<3>(rect);
    const int bufferSize = width * height * 3;
    std::vector<unsigned char> buffer(bufferSize);
    originScene->readBlock(rect, buffer.data(), buffer.size());
    cv::Mat originImage(height, width, CV_8UC3, buffer.data());

    MedianBlurFilter filter;
    const int kernelSize = 15;
    filter.setKernelSize(kernelSize);
    ConvolutionFilterScene scene(originScene->getCVScene(), filter);
    cv::Mat transformedImage;
    scene.appyTransformation(originImage, transformedImage);
    cv::Mat testImage;
    cv::medianBlur(originImage, testImage, kernelSize);
    TestTools::compareRasters(testImage, transformedImage);
}

TEST(ConvolutionFiltes, applyTransformationSobelFilter)
{
    std::string path = TestTools::getTestImagePath("gdal", "colors.png");
    std::shared_ptr<slideio::Slide> slide = openSlide(path, "AUTO");
    std::shared_ptr<slideio::Scene> originScene = slide->getScene(0);

    std::tuple<int, int, int, int> rect = originScene->getRect();
    const int width = std::get<2>(rect);
    const int height = std::get<3>(rect);
    const int bufferSize = width * height * 3;
    std::vector<unsigned char> buffer(bufferSize);
    originScene->readBlock(rect, buffer.data(), buffer.size());
    cv::Mat originImage(height, width, CV_8UC3, buffer.data());

    SobelFilter filter;
    const int kernelSize = 15;
    filter.setDepth(DataType::DT_Int16);
    filter.setKernelSize(kernelSize);
    filter.setDx(1);
    filter.setDy(0);
    ConvolutionFilterScene scene(originScene->getCVScene(), filter);
    cv::Mat transformedImage;
    scene.appyTransformation(originImage, transformedImage);
    cv::Mat testImage;
    cv::Sobel(originImage, testImage, CV_16S, 1, 0, kernelSize);
    TestTools::compareRasters(testImage, transformedImage);
    //TestTools::showRaster(transformedImage);
}

TEST(ConvolutionFiltes, applyTransformationSharrFilter)
{
    std::string path = TestTools::getTestImagePath("gdal", "colors.png");
    std::shared_ptr<slideio::Slide> slide = openSlide(path, "AUTO");
    std::shared_ptr<slideio::Scene> originScene = slide->getScene(0);

    std::tuple<int, int, int, int> rect = originScene->getRect();
    const int width = std::get<2>(rect);
    const int height = std::get<3>(rect);
    const int bufferSize = width * height * 3;
    std::vector<unsigned char> buffer(bufferSize);
    originScene->readBlock(rect, buffer.data(), buffer.size());
    cv::Mat originImage(height, width, CV_8UC3, buffer.data());

    ScharrFilter filter;
    filter.setDepth(DataType::DT_Int16);
    filter.setDx(1);
    filter.setDy(0);
    ConvolutionFilterScene scene(originScene->getCVScene(), filter);
    cv::Mat transformedImage;
    scene.appyTransformation(originImage, transformedImage);
    cv::Mat testImage;
    cv::Scharr(originImage, testImage, CV_16S, 1, 0);
    TestTools::compareRasters(testImage, transformedImage);
    //TestTools::showRaster(transformedImage);
}