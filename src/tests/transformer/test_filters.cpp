#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include "slideio/converter/converter.hpp"
#include "slideio/slideio/slideio.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/base/exceptions.hpp"
#include <boost/filesystem.hpp>
#include <opencv2/imgproc.hpp>

#include "slideio/core/tools/tools.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include "slideio/transformer/transformations.hpp"
#include "slideio/transformer/transformer.hpp"
#include "slideio/transformer/transformerscene.hpp"
#include "tests/testlib/testscene.hpp"

using namespace slideio;

TEST(Filters, applyTransformationGaussianBlur)
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

    std::shared_ptr<GaussianBlurFilter> filter(new GaussianBlurFilter);
    const int kernelSize = 15;
    filter->setKernelSizeX(kernelSize);
    filter->setKernelSizeY(kernelSize);
    cv::Mat transformedImage;
    filter->applyTransformation(originImage, transformedImage);
    cv::Mat testImage;
    cv::GaussianBlur(originImage, testImage, cv::Size(kernelSize, kernelSize), 0);
    TestTools::compareRasters(testImage, transformedImage);

}

TEST(Filters, applyTransformationMedianBlur)
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
    cv::Mat transformedImage;
    filter.applyTransformation(originImage, transformedImage);
    cv::Mat testImage;
    cv::medianBlur(originImage, testImage, kernelSize);
    TestTools::compareRasters(testImage, transformedImage);
}

TEST(Filters, applyTransformationSobelFilter)
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
    cv::Mat transformedImage;
    filter.applyTransformation(originImage, transformedImage);
    cv::Mat testImage;
    cv::Sobel(originImage, testImage, CV_16S, 1, 0, kernelSize);
    TestTools::compareRasters(testImage, transformedImage);
    //TestTools::showRaster(transformedImage);
}

TEST(Filters, applyTransformationSharrFilter)
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
    cv::Mat transformedImage;
    filter.applyTransformation(originImage, transformedImage);
    cv::Mat testImage;
    cv::Scharr(originImage, testImage, CV_16S, 1, 0);
    TestTools::compareRasters(testImage, transformedImage);
    //TestTools::showRaster(transformedImage);
}

TEST(Filters, readBlockWholeImageSobel)
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

TEST(Filters, readBlockPartialScharr)
{
    std::string path = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
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
    cv::Rect cvBlockRect(10, 15, 300, 350);
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

TEST(Filters, readScaledBlockMedian)
{
    // scale the image to 50% and apply median filter
    const double scale = 0.5;
    const int kernelSize = 5;

    std::string path = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
    std::shared_ptr<slideio::Slide> slide = openSlide(path, "AUTO");
    std::shared_ptr<slideio::Scene> originScene = slide->getScene(0);

    std::tuple<int, int, int, int> rect = originScene->getRect();
    const int width = std::get<2>(rect);
    const int height = std::get<3>(rect);

    // specify the block to read
    const cv::Rect cvBlockRect(width / 4, height / 4, width / 6, height / 6);
    // specify the block size after scaling
    const cv::Size cvBlockSize(std::lround(cvBlockRect.width * scale), std::lround(cvBlockRect.height * scale));
    // specify image size after scaling
    const cv::Size cvScaledImageSize(std::lround(width*scale), std::lround(height*scale));

    // read the original image and scale it
    const int bufferSize = cvScaledImageSize.width * cvScaledImageSize.height * 3;
    std::vector<unsigned char> buffer(bufferSize);
    auto imageSize = std::make_tuple(cvScaledImageSize.width, cvScaledImageSize.height);
    originScene->readResampledBlock(rect, imageSize, buffer.data(), buffer.size());
    cv::Mat scaledImage(cvScaledImageSize.height, cvScaledImageSize.width, CV_8UC3, buffer.data());
    // apply median filter to the scaled image
    cv::Mat testImage;
    cv::medianBlur(scaledImage, testImage, kernelSize);
    const cv::Rect cvTestRect(std::lround(cvBlockRect.x*scale),
        std::lround(cvBlockRect.y*scale),
        std::lround(cvBlockRect.width*scale),
        std::lround(cvBlockRect.height*scale));
    // extract the test image block
    cv::Mat testImageBlock(testImage, cvTestRect);

    MedianBlurFilter filter;
    filter.setKernelSize(kernelSize);
    std::shared_ptr<slideio::Scene> transformedScene = transformScene(originScene, filter);
    std::vector<unsigned char> transformedBuffer(cvBlockSize.width*cvBlockSize.height*3);
    auto blockRect = std::make_tuple(cvBlockRect.x, cvBlockRect.y, cvBlockRect.width, cvBlockRect.height);
    auto blockSize = std::make_tuple(cvBlockSize.width, cvBlockSize.height);
    transformedScene->readResampledBlock(blockRect, blockSize, transformedBuffer.data(), transformedBuffer.size());
    cv::Mat transformedImage(cvBlockSize.height, cvBlockSize.width, CV_8UC3, transformedBuffer.data());
    TestTools::compareRasters(testImageBlock, transformedImage);
    //TestTools::showRaster(scaledImage);
    //TestTools::showRaster(transformedImage);
}

TEST(Filters, readScaledBlockBilateral)
{
    // scale the image to 50% and apply bilateral filter
    const int diameter = 15;
    const double sigmaColor = 80.;
    const double sigmaSpace = 80.;
    const double scale = 0.5;

    std::string path = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
    std::shared_ptr<slideio::Slide> slide = openSlide(path, "AUTO");
    std::shared_ptr<slideio::Scene> originScene = slide->getScene(0);

    std::tuple<int, int, int, int> rect = originScene->getRect();
    const int width = std::get<2>(rect);
    const int height = std::get<3>(rect);

    // specify the block to read
    const cv::Rect cvBlockRect(width / 4, height / 4, width / 6, height / 6);
    // specify the block size after scaling
    const cv::Size cvBlockSize(std::lround(cvBlockRect.width * scale), std::lround(cvBlockRect.height * scale));
    // specify image size after scaling
    const cv::Size cvScaledImageSize(std::lround(width * scale), std::lround(height * scale));

    // read the original image and scale it
    const int bufferSize = cvScaledImageSize.width * cvScaledImageSize.height * 3;
    std::vector<unsigned char> buffer(bufferSize);
    auto imageSize = std::make_tuple(cvScaledImageSize.width, cvScaledImageSize.height);
    originScene->readResampledBlock(rect, imageSize, buffer.data(), buffer.size());
    cv::Mat scaledImage(cvScaledImageSize.height, cvScaledImageSize.width, CV_8UC3, buffer.data());
    // apply median filter to the scaled image
    cv::Mat testImage;
    cv::bilateralFilter(scaledImage, testImage, diameter, sigmaColor, sigmaSpace);
    const cv::Rect cvTestRect(std::lround(cvBlockRect.x * scale),
        std::lround(cvBlockRect.y * scale),
        std::lround(cvBlockRect.width * scale),
        std::lround(cvBlockRect.height * scale));
    // extract the test image block
    cv::Mat testImageBlock(testImage, cvTestRect);

    BilateralFilter filter;
    filter.setDiameter(diameter);
    filter.setSigmaColor(sigmaColor);
    filter.setSigmaSpace(sigmaSpace);
    std::shared_ptr<slideio::Scene> transformedScene = transformScene(originScene, filter);
    std::vector<unsigned char> transformedBuffer(cvBlockSize.width * cvBlockSize.height * 3);
    auto blockRect = std::make_tuple(cvBlockRect.x, cvBlockRect.y, cvBlockRect.width, cvBlockRect.height);
    auto blockSize = std::make_tuple(cvBlockSize.width, cvBlockSize.height);
    transformedScene->readResampledBlock(blockRect, blockSize, transformedBuffer.data(), transformedBuffer.size());
    cv::Mat transformedImage(cvBlockSize.height, cvBlockSize.width, CV_8UC3, transformedBuffer.data());
    TestTools::compareRasters(testImageBlock, transformedImage);
    //TestTools::showRaster(transformedImage);
}

TEST(Filters, readScaledBlockGaussianBlur)
{
    // scale the image to 50% and apply Gaussian blur filter
    const int kernelSize = 7;
    const double sigmaX = 0.;
    const double sigmaY = 0.;
    const double scale = 0.5;

    std::string path = TestTools::getTestImagePath("gdal", "colors.png");
    std::shared_ptr<slideio::Slide> slide = openSlide(path, "AUTO");
    std::shared_ptr<slideio::Scene> originScene = slide->getScene(0);

    std::tuple<int, int, int, int> rect = originScene->getRect();
    const int width = std::get<2>(rect);
    const int height = std::get<3>(rect);

    // specify the block to read
    const cv::Rect cvBlockRect(width / 4, height / 4, width / 2, height / 2);
    // specify the block size after scaling
    const cv::Size cvBlockSize(std::lround(cvBlockRect.width * scale), std::lround(cvBlockRect.height * scale));
    // specify image size after scaling
    const cv::Size cvScaledImageSize(std::lround(width * scale), std::lround(height * scale));

    // read the original image and scale it
    const int bufferSize = cvScaledImageSize.width * cvScaledImageSize.height * 3;
    std::vector<unsigned char> buffer(bufferSize);
    auto imageSize = std::make_tuple(cvScaledImageSize.width, cvScaledImageSize.height);
    originScene->readResampledBlock(rect, imageSize, buffer.data(), buffer.size());
    cv::Mat scaledImage(cvScaledImageSize.height, cvScaledImageSize.width, CV_8UC3, buffer.data());
    // apply median filter to the scaled image
    cv::Mat testImage;
    cv::GaussianBlur(scaledImage, testImage, cv::Size(kernelSize,kernelSize), sigmaX, sigmaY);
    const cv::Rect cvTestRect(std::lround(cvBlockRect.x * scale),
        std::lround(cvBlockRect.y * scale),
        std::lround(cvBlockRect.width * scale),
        std::lround(cvBlockRect.height * scale));
    // extract the test image block
    cv::Mat testImageBlock(testImage, cvTestRect);

    GaussianBlurFilter filter;
    filter.setKernelSizeX(kernelSize);
    filter.setKernelSizeY(kernelSize);
    filter.setSigmaX(sigmaX);
    filter.setSigmaY(sigmaY);
    std::shared_ptr<slideio::Scene> transformedScene = transformScene(originScene, filter);
    std::vector<unsigned char> transformedBuffer(cvBlockSize.width * cvBlockSize.height * 3);
    auto blockRect = std::make_tuple(cvBlockRect.x, cvBlockRect.y, cvBlockRect.width, cvBlockRect.height);
    auto blockSize = std::make_tuple(cvBlockSize.width, cvBlockSize.height);
    transformedScene->readResampledBlock(blockRect, blockSize, transformedBuffer.data(), transformedBuffer.size());
    cv::Mat transformedImage(cvBlockSize.height, cvBlockSize.width, CV_8UC3, transformedBuffer.data());
    TestTools::compareRasters(testImageBlock, transformedImage);
    //TestTools::showRaster(transformedImage);
}

TEST(Filters, readScaledBlockCanny)
{
    // scale the image to 50% and apply Canny filter
    const double threshold1 = 100;
    const double threshold2 = 150.;
    const int appertureSize = 5;
    bool L2gradient = false;
    const double scale = 0.5;

    std::string path = TestTools::getTestImagePath("gdal", "colors.png");
    std::shared_ptr<slideio::Slide> slide = openSlide(path, "AUTO");
    std::shared_ptr<slideio::Scene> originScene = slide->getScene(0);

    std::tuple<int, int, int, int> rect = originScene->getRect();
    const int width = std::get<2>(rect);
    const int height = std::get<3>(rect);

    // specify the block to read
    const cv::Rect cvBlockRect(width / 4, height / 4, width / 2, height / 2);
    // specify the block size after scaling
    const cv::Size cvBlockSize(std::lround(cvBlockRect.width * scale), std::lround(cvBlockRect.height * scale));
    // specify image size after scaling
    const cv::Size cvScaledImageSize(std::lround(width * scale), std::lround(height * scale));

    // read the original image and scale it
    const int bufferSize = cvScaledImageSize.width * cvScaledImageSize.height * 3;
    std::vector<unsigned char> buffer(bufferSize);
    auto imageSize = std::make_tuple(cvScaledImageSize.width, cvScaledImageSize.height);
    originScene->readResampledBlock(rect, imageSize, buffer.data(), buffer.size());
    cv::Mat scaledImage(cvScaledImageSize.height, cvScaledImageSize.width, CV_8UC3, buffer.data());
    // apply median filter to the scaled image
    cv::Mat testImage;
    cv::Canny(scaledImage, testImage, threshold1, threshold2, appertureSize, L2gradient);
    auto dep = testImage.depth();
    auto ch = testImage.channels();
    const cv::Rect cvTestRect(std::lround(cvBlockRect.x * scale),
        std::lround(cvBlockRect.y * scale),
        std::lround(cvBlockRect.width * scale),
        std::lround(cvBlockRect.height * scale));
    // extract the test image block
    cv::Mat testImageBlock(testImage, cvTestRect);

    CannyFilter filter;
    filter.setThreshold1(threshold1);
    filter.setThreshold2(threshold2);
    filter.setApertureSize(appertureSize);
    filter.setL2Gradient(L2gradient);
    std::shared_ptr<slideio::Scene> transformedScene = transformScene(originScene, filter);
    std::vector<unsigned char> transformedBuffer(cvBlockSize.width * cvBlockSize.height * 3);
    auto blockRect = std::make_tuple(cvBlockRect.x, cvBlockRect.y, cvBlockRect.width, cvBlockRect.height);
    auto blockSize = std::make_tuple(cvBlockSize.width, cvBlockSize.height);
    transformedScene->readResampledBlock(blockRect, blockSize, transformedBuffer.data(), transformedBuffer.size());
    cv::Mat transformedImage(cvBlockSize.height, cvBlockSize.width, CV_8U, transformedBuffer.data());
    auto dep2 = transformedImage.depth();
    auto ch2 = transformedImage.channels();
    TestTools::compareRasters(testImageBlock, transformedImage);
    cv::Mat diff = (testImageBlock != transformedImage);
    //TestTools::showRaster(diff);
}
