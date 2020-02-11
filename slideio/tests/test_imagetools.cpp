#include <gtest/gtest.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "testtools.hpp"
#include <fstream>

TEST(ImageTools, readJp2KFile)
{
    std::string filePath = TestTools::getTestImagePath("jp2K","relax.jp2");
    std::string bmpFilePath = TestTools::getTestImagePath("jp2K","relax.bmp");
    // read jp2k file
    cv::Mat jp2k;
    slideio::ImageTools::readJp2KFile(filePath, jp2k);
    EXPECT_EQ(300, jp2k.rows);
    EXPECT_EQ(400, jp2k.cols);
    EXPECT_EQ(3, jp2k.channels());
    // read bmp file (conversion from jp2k)
    cv::Mat bmp;
    slideio::ImageTools::readGDALImage(bmpFilePath, bmp);
    // compare similarity of rasters from bmp and decoded jp2k file
    cv::Mat score;
    cv::matchTemplate(bmp, jp2k, score, cv::TM_CCOEFF_NORMED);
    double minScore(0), maxScore(0);
    cv::minMaxLoc(score, &minScore, &maxScore);
    ASSERT_LT(0.99, minScore);
}

TEST(ImageTools, readGDALImage)
{
    std::string path = TestTools::getTestImagePath("gdal","img_1024x600_3x8bit_RGB_color_bars_CMYKWRGB.png");
    cv::Mat image;
    slideio::ImageTools::readGDALImage(path, image);
    cv::Rect rect(260, 500, 100, 100);
    cv::Mat block(image, rect);

    ASSERT_FALSE(image.empty());
    int channels = image.channels();
    EXPECT_EQ(channels, 3);
    cv::Size size = {image.cols, image.rows};
    EXPECT_EQ(size.width, 1024);
    EXPECT_EQ(size.height, 600);

    cv::Mat channelRaster[3];
    for(int channel=0; channel<channels; channel++)
    {
        cv::extractChannel(block, channelRaster[channel], channel);
    }

    double minVal(0), maxVal(0);
    cv::minMaxLoc(channelRaster[0], &minVal, &maxVal);
    EXPECT_EQ(minVal,255);
    EXPECT_EQ(maxVal,255);

    cv::minMaxLoc(channelRaster[1], &minVal, &maxVal);
    EXPECT_EQ(minVal,255);
    EXPECT_EQ(maxVal,255);

    cv::minMaxLoc(channelRaster[2], &minVal, &maxVal);
    EXPECT_EQ(minVal,0);
    EXPECT_EQ(maxVal,0);
}

TEST(ImageTools, readJxrImage)
{
    std::string pathJxr = TestTools::getTestImagePath("jxr","seagull.wdp");
    std::string pathBmp = TestTools::getTestImagePath("jxr","seagull.bmp");
    cv::Mat jxrImage, bmpImage;
    slideio::ImageTools::readJxrImage(pathJxr, jxrImage);
    slideio::ImageTools::readGDALImage(pathBmp, bmpImage);

    // compare similarity of rasters from bmp and decoded jpxr file
    cv::Mat score;
    cv::matchTemplate(jxrImage, bmpImage, score, cv::TM_CCOEFF_NORMED);
    double minScore(0), maxScore(0);
    cv::minMaxLoc(score, &minScore, &maxScore);
    ASSERT_LT(0.99, minScore);
}

TEST(ImageTools, decodeJxrBlock)
{
    std::string pathJxr = TestTools::getTestImagePath("jxr","seagull.wdp");
    std::string pathBmp = TestTools::getTestImagePath("jxr","seagull.bmp");
    // read jpxr file to the memory
    std::ifstream file (pathJxr, std::ios::in| std::ios::binary| std::ios::ate);
    ASSERT_TRUE(file.is_open());
    size_t size = file.tellg();
    ASSERT_GT(size, 0);
    std::vector<uint8_t> buffer(size);
    file.seekg(0,std::ios::beg);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    file.close();
    // decode the codeblock
    cv::Mat jxrImage;
    slideio::ImageTools::decodeJxrBlock(buffer.data(), buffer.size(), jxrImage);
    ASSERT_FALSE(jxrImage.empty());
    cv::Mat bmpImage;
    slideio::ImageTools::readGDALImage(pathBmp, bmpImage);
    ASSERT_FALSE(bmpImage.empty());
    // compare similarity of rasters from bmp and decoded jpxr file
    cv::Mat score;
    cv::matchTemplate(jxrImage, bmpImage, score, cv::TM_CCOEFF_NORMED);
    double minScore(0), maxScore(0);
    cv::minMaxLoc(score, &minScore, &maxScore);
    ASSERT_LT(0.99, minScore);
}