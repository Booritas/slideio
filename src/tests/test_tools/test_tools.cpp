#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include <opencv2/imgproc.hpp>
#include "slideio/core/tools/tempfile.hpp"

TEST(TestTools, writeReadRawImage_8UC1) {
    // Create a test image with CV_8UC1 (8-bit unsigned, 1 channel)
    cv::Mat original(100, 100, CV_8UC1);
    cv::randu(original, cv::Scalar(0), cv::Scalar(255));
    
    // Write the image to a temporary file
    slideio::TempFile tempFile("test_raw_%%%%");
    std::string filePath = tempFile.getPath().string();
    TestTools::writeRawImage(filePath, original);
    
    // Read the image back
    cv::Mat loaded(100, 100, CV_8UC1);
    TestTools::readRawImage(filePath, loaded);
    
    // Compare the images
    TestTools::compareRasters(original, loaded);
}

TEST(TestTools, writeReadRawImage_8UC3) {
    // Create a test image with CV_8UC3 (8-bit unsigned, 3 channels - RGB)
    cv::Mat original(100, 150, CV_8UC3);
    cv::randu(original, cv::Scalar(0, 0, 0), cv::Scalar(255, 255, 255));
    
    // Write the image to a temporary file
    slideio::TempFile tempFile("test_raw_%%%%");
    std::string filePath = tempFile.getPath().string();
    TestTools::writeRawImage(filePath, original);
    
    // Read the image back
    cv::Mat loaded(100, 150, CV_8UC3);
    TestTools::readRawImage(filePath, loaded);
    
    // Compare the images
    TestTools::compareRasters(original, loaded);
}

TEST(TestTools, writeReadRawImage_16UC1) {
    // Create a test image with CV_16UC1 (16-bit unsigned, 1 channel)
    cv::Mat original(80, 120, CV_16UC1);
    cv::randu(original, cv::Scalar(0), cv::Scalar(65535));
    
    // Write the image to a temporary file
    slideio::TempFile tempFile("test_raw_%%%%");
    std::string filePath = tempFile.getPath().string();
    TestTools::writeRawImage(filePath, original);
    
    // Read the image back
    cv::Mat loaded(80, 120, CV_16UC1);
    TestTools::readRawImage(filePath, loaded);
    
    // Compare the images
    TestTools::compareRasters(original, loaded);
}

TEST(TestTools, writeReadRawImage_16UC3) {
    // Create a test image with CV_16UC3 (16-bit unsigned, 3 channels)
    cv::Mat original(60, 90, CV_16UC3);
    cv::randu(original, cv::Scalar(0, 0, 0), cv::Scalar(65535, 65535, 65535));
    
    // Write the image to a temporary file
    slideio::TempFile tempFile("test_raw_%%%%");
    std::string filePath = tempFile.getPath().string();
    TestTools::writeRawImage(filePath, original);
    
    // Read the image back
    cv::Mat loaded(60, 90, CV_16UC3);
    TestTools::readRawImage(filePath, loaded);
    
    // Compare the images
    TestTools::compareRasters(original, loaded);
}

TEST(TestTools, writeReadRawImage_32FC1) {
    // Create a test image with CV_32FC1 (32-bit float, 1 channel)
    cv::Mat original(50, 75, CV_32FC1);
    cv::randu(original, cv::Scalar(0.0f), cv::Scalar(1.0f));
    
    // Write the image to a temporary file
    slideio::TempFile tempFile("test_raw_%%%%");
    std::string filePath = tempFile.getPath().string();
    TestTools::writeRawImage(filePath, original);
    
    // Read the image back
    cv::Mat loaded(50, 75, CV_32FC1);
    TestTools::readRawImage(filePath, loaded);
    
    // Compare the images
    TestTools::compareRasters(original, loaded);
}

TEST(TestTools, writeReadRawImage_32FC3) {
    // Create a test image with CV_32FC3 (32-bit float, 3 channels)
    cv::Mat original(40, 60, CV_32FC3);
    cv::randu(original, cv::Scalar(0.0f, 0.0f, 0.0f), cv::Scalar(1.0f, 1.0f, 1.0f));
    
    // Write the image to a temporary file
    slideio::TempFile tempFile("test_raw_%%%%");
    std::string filePath = tempFile.getPath().string();
    TestTools::writeRawImage(filePath, original);
    
    // Read the image back
    cv::Mat loaded(40, 60, CV_32FC3);
    TestTools::readRawImage(filePath, loaded);
    
    // Compare the images
    TestTools::compareRasters(original, loaded);
}

TEST(TestTools, writeReadRawImage_Pattern) {
    // Create a test image with a specific pattern
    cv::Mat original(100, 100, CV_8UC3);
    
    // Create a checkerboard pattern
    for (int y = 0; y < original.rows; ++y) {
        for (int x = 0; x < original.cols; ++x) {
            if ((x / 10 + y / 10) % 2 == 0) {
                original.at<cv::Vec3b>(y, x) = cv::Vec3b(255, 0, 0); // Blue
            } else {
                original.at<cv::Vec3b>(y, x) = cv::Vec3b(0, 255, 0); // Green
            }
        }
    }
    
    // Write the image to a temporary file
    slideio::TempFile tempFile("test_raw_%%%%");
    std::string filePath = tempFile.getPath().string();
    TestTools::writeRawImage(filePath, original);
    
    // Read the image back
    cv::Mat loaded(100, 100, CV_8UC3);
    TestTools::readRawImage(filePath, loaded);
    
    // Compare the images
    TestTools::compareRasters(original, loaded);
}

TEST(TestTools, writeReadRawImage_ZeroImage) {
    // Create a zero-filled image
    cv::Mat original = cv::Mat::zeros(100, 100, CV_16UC1);
    
    // Write the image to a temporary file
    slideio::TempFile tempFile("test_raw_%%%%");
    std::string filePath = tempFile.getPath().string();
    TestTools::writeRawImage(filePath, original);
    
    // Read the image back
    cv::Mat loaded(100, 100, CV_16UC1);
    TestTools::readRawImage(filePath, loaded);
    
    // Compare the images
    TestTools::compareRasters(original, loaded);
    EXPECT_TRUE(TestTools::isRasterEmpty(loaded));
}

TEST(TestTools, writeReadRawImage_MaxValues) {
    // Create an image with maximum values
    cv::Mat original = cv::Mat::ones(50, 50, CV_8UC1) * 255;
    
    // Write the image to a temporary file
    slideio::TempFile tempFile("test_raw_%%%%");
    std::string filePath = tempFile.getPath().string();
    TestTools::writeRawImage(filePath, original);
    
    // Read the image back
    cv::Mat loaded(50, 50, CV_8UC1);
    TestTools::readRawImage(filePath, loaded);
    
    // Compare the images
    TestTools::compareRasters(original, loaded);
}

TEST(TestTools, writeReadRawImage_MultiChannel) {
    // Create a test image with CV_8UC4 (8-bit unsigned, 4 channels - RGBA)
    cv::Mat original(64, 64, CV_8UC4);
    cv::randu(original, cv::Scalar(0, 0, 0, 0), cv::Scalar(255, 255, 255, 255));
    
    // Write the image to a temporary file
    slideio::TempFile tempFile("test_raw_%%%%");
    std::string filePath = tempFile.getPath().string();
    TestTools::writeRawImage(filePath, original);
    
    // Read the image back
    cv::Mat loaded(64, 64, CV_8UC4);
    TestTools::readRawImage(filePath, loaded);
    
    // Compare the images
    TestTools::compareRasters(original, loaded);
}
