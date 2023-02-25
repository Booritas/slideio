#include <gtest/gtest.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/drivers/gdal/gdalimagedriver.hpp"
#include "tests/testlib/testtools.hpp"
#include <fstream>

#include "slideio/imagetools/tempfile.hpp"

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

TEST(ImageTools, writeRGBImage)
{
    std::string pathBmp = TestTools::getTestImagePath("jxr","seagull.bmp");
    slideio::TempFile pathPng("png");
    cv::Mat sourceRaster;
    slideio::ImageTools::readGDALImage(pathBmp, sourceRaster);
    slideio::ImageTools::writeRGBImage(pathPng.getPath().string(), slideio::Compression::Png, sourceRaster);
    slideio::GDALImageDriver drv;
    auto slide = drv.openFile(pathPng.getPath().string());
    auto scene = slide->getScene(0);
    cv::Mat destRaster;
    auto rect = scene->getRect();
    auto compression = scene->getCompression();
    EXPECT_EQ(rect.width, sourceRaster.cols);
    EXPECT_EQ(rect.height, sourceRaster.rows);
    EXPECT_EQ(compression, slideio::Compression::Png);
    scene->readBlock(rect, destRaster);
    cv::Mat diffRaster = sourceRaster - destRaster;
    double minVal(0), maxVal(0);
    cv::minMaxLoc(diffRaster, &minVal, &maxVal);
    EXPECT_EQ(minVal, 0);
    EXPECT_EQ(maxVal, 0);
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

TEST(ImageTools, readJxrImageCorrupted)
{
    std::string pathJxr = TestTools::getTestImagePath("jxr","corrupted.wdp");
    cv::Mat jxrImage, bmpImage;
    EXPECT_THROW(slideio::ImageTools::readJxrImage(pathJxr, jxrImage), std::exception);
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

TEST(ImageTools, decodeJxrBlock16)
{
    std::string pathJxr = TestTools::getTestImagePath("jxr","tile16.jxr");
    std::string pathRaw = TestTools::getTestImagePath("jxr","tile16.raw");
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


    std::ifstream rawFile(pathRaw.c_str(), std::ios::binary);
    std::vector<unsigned char> raw((std::istreambuf_iterator<char>(rawFile)), std::istreambuf_iterator<char>());
    size_t rasterSize = jxrImage.total() * jxrImage.elemSize();
    ASSERT_EQ(rasterSize, raw.size());
    ASSERT_EQ(memcmp(jxrImage.data, raw.data(), raw.size()), 0);
}

TEST(ImageTools, decodeJpegStream)
{
    std::string pathJpg = TestTools::getTestImagePath("jpeg", "lena_256.jpg");
    std::string pathPng = TestTools::getTestImagePath("jpeg", "lena_256.png");

    std::ifstream file(pathJpg, std::ios::in | std::ios::binary | std::ios::ate);
    ASSERT_TRUE(file.is_open());
    size_t size = file.tellg();
    ASSERT_GT(size, 0);
    std::vector<uint8_t> buffer(size);
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    file.close();
    // decode the codeblock
    cv::Mat jpegImage;
    slideio::ImageTools::decodeJpegStream(buffer.data(), buffer.size(), jpegImage);
    ASSERT_FALSE(jpegImage.empty());
    cv::Mat bmpImage;
    slideio::ImageTools::readGDALImage(pathPng, bmpImage);
    ASSERT_FALSE(bmpImage.empty());
    // compare similarity of rasters from bmp and decoded jpeg file
    cv::Mat score;
    cv::matchTemplate(jpegImage, bmpImage, score, cv::TM_CCOEFF_NORMED);
    double minScore(0), maxScore(0);
    cv::minMaxLoc(score, &minScore, &maxScore);
    ASSERT_LT(0.999, minScore);
}

TEST(ImageTools, decodeJpegStream2)
{
    std::string pathJpg = TestTools::getTestImagePath("jpeg", "p2YCpvg.jpeg");
    std::string pathPng = TestTools::getTestImagePath("jpeg", "p2YCpvg.png");

    std::ifstream file(pathJpg, std::ios::in | std::ios::binary | std::ios::ate);
    ASSERT_TRUE(file.is_open());
    size_t size = file.tellg();
    ASSERT_GT(size, 0);
    std::vector<uint8_t> buffer(size);
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    file.close();
    // decode the codeblock
    cv::Mat jpegImage;
    slideio::ImageTools::decodeJpegStream(buffer.data(), buffer.size(), jpegImage);
    ASSERT_FALSE(jpegImage.empty());
    cv::Mat bmpImage;
    slideio::ImageTools::readGDALImage(pathPng, bmpImage);
    ASSERT_FALSE(bmpImage.empty());
    // compare similarity of rasters from bmp and decoded jpeg file
    cv::Mat score;
    cv::matchTemplate(jpegImage, bmpImage, score, cv::TM_CCOEFF_NORMED);
    double minScore(0), maxScore(0);
    cv::minMaxLoc(score, &minScore, &maxScore);
    ASSERT_LT(0.999, minScore);
}

TEST(ImageTools, computeSimilarityEqual)
{
    cv::Mat left(100, 200, CV_16SC1, cv::Scalar((short)55));
    cv::Mat right(100, 200, CV_16SC1, cv::Scalar((short)55));
    double similarity = slideio::ImageTools::computeSimilarity(left, right);
    EXPECT_DOUBLE_EQ(similarity, 1.);
}

TEST(ImageTools, computeSimilarityDifferent)
{
    cv::Mat left(100, 200, CV_16SC1);
    cv::Mat right(100, 200, CV_16SC1);

    double mean = 0.0;
    double stddev = 500.0 / 3.0;
    cv::randn(left, cv::Scalar(mean), cv::Scalar(stddev));
    cv::randu(right, cv::Scalar(-500), cv::Scalar(500));

    double similarity = slideio::ImageTools::computeSimilarity(left, right);
    EXPECT_LT(similarity, 0.6);
}

TEST(ImageTools, computeSimilaritySimilar)
{
    std::string pathPng = TestTools::getTestImagePath("jpeg", "lena_256.png");
    cv::Mat left;
    slideio::ImageTools::readGDALImage(pathPng, left);
    cv::Mat right = left.clone();
    cv::blur(right, right, cv::Size(3, 3));
    double similarity = slideio::ImageTools::computeSimilarity(left, right);
    EXPECT_GT(similarity, 0.9);
}

TEST(ImageTools, computeSimilaritySimilar2)
{
    std::string pathPng = TestTools::getTestImagePath("jpeg", "lena_256.png");
    cv::Mat left;
    slideio::ImageTools::readGDALImage(pathPng, left);
    cv::Mat right = left.clone();
    cv::Point center(right.cols / 2, right.rows / 2);
    cv::circle(right, center, 75, cv::Scalar(0, 0, 0), cv::FILLED);
    double similarity = slideio::ImageTools::computeSimilarity(left, right);
    EXPECT_LT(similarity, 0.75);
    EXPECT_GT(similarity, 0.5);
}

TEST(ImageTools, encodeJpeg)
{
    std::string pathPng = TestTools::getTestImagePath("jpeg", "lena_256.png");
    cv::Mat source;
    slideio::ImageTools::readGDALImage(pathPng, source);
    std::vector<uint8_t> output;
    slideio::ImageTools::encodeJpeg(source, output, 99);
    slideio::TempFile jpeg("jpg");
    std::string pathJpeg = jpeg.getPath().string();
    std::ofstream file(pathJpeg, std::ios::binary);
    ASSERT_TRUE(file.is_open());
    file.write(reinterpret_cast<const char*>(output.data()), output.size());
    file.close();
    cv::Mat target;
    slideio::ImageTools::readGDALImage(pathJpeg, target);
    double similarity = slideio::ImageTools::computeSimilarity(source, target);
    EXPECT_GE(similarity, 0.99);
}