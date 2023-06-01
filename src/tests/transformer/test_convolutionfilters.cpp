#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include "slideio/converter/converter.hpp"
#include "slideio/slideio/slideio.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/base/exceptions.hpp"
#include <boost/filesystem.hpp>
#include <opencv2/imgproc.hpp>

#include "slideio/core/tools/tools.hpp"
#include "slideio/imagetools/cvtools.hpp"
#include "slideio/transformer/colortransformation.hpp"
#include "slideio/transformer/convolutionfilter.hpp"
#include "slideio/transformer/convolutionfilterscene.hpp"
#include "slideio/transformer/transformer.hpp"
#include "tests/testlib/testscene.hpp"

using namespace slideio;


TEST(ConvolutionFilters, applyTransformationGaussianBlur)
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
    scene.applyTransformation(originImage, transformedImage);
    cv::Mat testImage;
    cv::GaussianBlur(originImage, testImage, cv::Size(kernelSize, kernelSize), 0);
    TestTools::compareRasters(testImage, transformedImage);

}

TEST(ConvolutionFilters, applyTransformationMedianBlur)
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
    scene.applyTransformation(originImage, transformedImage);
    cv::Mat testImage;
    cv::medianBlur(originImage, testImage, kernelSize);
    TestTools::compareRasters(testImage, transformedImage);
}

TEST(ConvolutionFilters, applyTransformationSobelFilter)
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
    scene.applyTransformation(originImage, transformedImage);
    cv::Mat testImage;
    cv::Sobel(originImage, testImage, CV_16S, 1, 0, kernelSize);
    TestTools::compareRasters(testImage, transformedImage);
    //TestTools::showRaster(transformedImage);
}

TEST(ConvolutionFilters, applyTransformationSharrFilter)
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
    scene.applyTransformation(originImage, transformedImage);
    cv::Mat testImage;
    cv::Scharr(originImage, testImage, CV_16S, 1, 0);
    TestTools::compareRasters(testImage, transformedImage);
    //TestTools::showRaster(transformedImage);
}

TEST(ConvolutionFilters, readBlockWholeImageSobel)
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
    DataType dt = DataType::DT_Int16;
    int cvType = CVTools::cvTypeFromDataType(dt);
    filter.setDepth(dt);
    filter.setKernelSize(kernelSize);
    filter.setDx(1);
    filter.setDy(0);
    std::shared_ptr<slideio::Scene> transformedScene = transformScene(originScene, filter);
    std::vector<unsigned char> transformedBuffer(width*height*originScene->getNumChannels()*CVTools::cvGetDataTypeSize(dt));
    transformedScene->readBlock(rect, transformedBuffer.data(), transformedBuffer.size());
    cv::Mat transformedImage(height, width, CV_MAKETYPE(cvType,originScene->getNumChannels()), transformedBuffer.data());
    cv::Mat testImage;
    cv::Sobel(originImage, testImage, cvType, 1, 0, kernelSize);
    auto testDepth = testImage.depth();
    auto transformedDepth = transformedImage.depth();
    ASSERT_EQ(testDepth, transformedDepth);
    ASSERT_EQ(testImage.channels(), transformedImage.channels());
    TestTools::compareRasters(testImage, transformedImage);
    //TestTools::showRaster(transformedImage);
}

TEST(ConvolutionFilters, readBlockPartialScharr)
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
    DataType dt = DataType::DT_Int16;
    int cvType = CVTools::cvTypeFromDataType(dt);
    filter.setDepth(dt);
    filter.setDx(1);
    filter.setDy(0);
    std::shared_ptr<Scene> transformedScene = transformScene(originScene, filter);
    std::vector<unsigned char> transformedBuffer(width * height * originScene->getNumChannels() * CVTools::cvGetDataTypeSize(dt));
    cv::Rect cvBlockRect(10, 15, 100, 150);
    std::tuple<int, int, int, int> blockRect(cvBlockRect.x, cvBlockRect.y, cvBlockRect.width, cvBlockRect.height);
    transformedScene->readBlock(blockRect, transformedBuffer.data(), transformedBuffer.size());
    cv::Mat transformedImage(cvBlockRect.height, cvBlockRect.width, CV_MAKETYPE(cvType, originScene->getNumChannels()), transformedBuffer.data());
    cv::Mat testImage;
    cv::Scharr(originImage, testImage, cvType, 1, 0);
    cv::Mat testImageBlock(testImage, cvBlockRect);
    auto testDepth = testImage.depth();
    auto transformedDepth = transformedImage.depth();
    ASSERT_EQ(testDepth, transformedDepth);
    ASSERT_EQ(testImage.channels(), transformedImage.channels());
    TestTools::compareRasters(testImageBlock, transformedImage);
    //TestTools::showRaster(transformedImage);
}
