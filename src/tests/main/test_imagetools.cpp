#include <gtest/gtest.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/drivers/gdal/gdalimagedriver.hpp"
#include "tests/testlib/testtools.hpp"
#include <fstream>

#include "slideio/core/tools/tempfile.hpp"

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

TEST(ImageTools, readJp2Header)
{
    std::string filePath = TestTools::getTestImagePath("jp2K", "relax.jp2");
    std::string bmpFilePath = TestTools::getTestImagePath("jp2K", "relax.bmp");

    auto fileSize = boost::filesystem::file_size(filePath);
    ASSERT_GT(fileSize, 0);
    std::ifstream file(filePath, std::ios::binary);
    // Stop eating new lines in binary mode!!!
    file.unsetf(std::ios::skipws);
    // reserve capacity
    std::vector<uint8_t> vec;
    vec.reserve(fileSize);
    // read the data:
    vec.insert(vec.begin(),
        std::istream_iterator<uint8_t>(file),
        std::istream_iterator<uint8_t>());
    slideio::ImageTools::ImageHeader header;
    slideio::ImageTools::readJp2KStremHeader(vec.data(), vec.size(), header);
    EXPECT_EQ(300, header.size.height);
    EXPECT_EQ(400, header.size.width);
    EXPECT_EQ(3, header.channels);
    EXPECT_EQ(3, header.chanelTypes.size());
    for(int i=0;i<3;i++) {
        EXPECT_EQ(CV_8U, header.chanelTypes[i]);
    }
    file.close();
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
    std::string pathPng = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    cv::Mat source;
    slideio::ImageTools::readGDALImage(pathPng, source);
    std::vector<uint8_t> output;
    slideio::JpegEncodeParameters encodeParameters(99);
    slideio::ImageTools::encodeJpeg(source, output, encodeParameters);
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

TEST(ImageTools, encodeJpegGray)
{
    std::string pathPng = TestTools::getTestImagePath("gdal", "img_2448x2448_1x8bit_SRC_GRAY_ducks.png");
    cv::Mat source;
    slideio::ImageTools::readGDALImage(pathPng, source);
    std::vector<uint8_t> output;
    slideio::JpegEncodeParameters params;
    params.setQuality(99);
    slideio::ImageTools::encodeJpeg(source, output, params);
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

TEST(ConverterTools, ConvertTo32BitChannelsTest) {
    const int width = 3;
    const int height = 2;
    const int numChannels = 3;

    // Input data
    uint8_t inputData[] = {
        255, 0, 0,  // Red
        0, 255, 0,  // Green
        0, 0, 255,  // Blue
        128, 128, 128,  // Gray
        255, 255, 0,  // Yellow
        0, 255, 255   // Cyan
    };

    // Expected output data
    int32_t expectedChannel1[] = { 255, 0, 0,  128, 255, 0 };
    int32_t expectedChannel2[] = { 0, 255, 0,  128, 255, 255 };
    int32_t expectedChannel3[] = { 0, 0, 255,  128, 0, 255 };

    // Initialize channel pointers
    int32_t* channel1 = new int32_t[width * height];
    int32_t* channel2 = new int32_t[width * height];
    int32_t* channel3 = new int32_t[width * height];
    int32_t* channels[] = { channel1, channel2, channel3 };

    slideio::ImageTools::convertTo32bitChannels(inputData, width, height, numChannels, channels);

    // Check the results
    for (int i = 0; i < width * height; i++) {
        EXPECT_EQ(expectedChannel1[i], channel1[i]) << "Error at index " << i << " for channel 1";
        EXPECT_EQ(expectedChannel2[i], channel2[i]) << "Error at index " << i << " for channel 2";
        EXPECT_EQ(expectedChannel3[i], channel3[i]) << "Error at index " << i << " for channel 3";
    }

    // Clean up memory
    delete[] channel1;
    delete[] channel2;
    delete[] channel3;
}

TEST(ImageTools, computeSimilarity2)
{
    std::string testFilePath1 = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-1-dir.png");
    std::string testFilePath2 = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-1-dir1.png");
    cv::Mat testRaster1;
    TestTools::readPNG(testFilePath1, testRaster1);
    cv::cvtColor(testRaster1, testRaster1, cv::COLOR_BGRA2BGR);
    cv::Mat testRaster2;
    TestTools::readPNG(testFilePath2, testRaster2);
    double similarity = slideio::ImageTools::computeSimilarity2(testRaster1, testRaster2);
    EXPECT_GT(similarity, 0.9);
}

TEST(ImageTools, computeSimilarity2Equal)
{
    cv::Mat left(100, 200, CV_16SC1, cv::Scalar((short)55));
    cv::Mat right(100, 200, CV_16SC1, cv::Scalar((short)55));
    double similarity = slideio::ImageTools::computeSimilarity2(left, right);
    EXPECT_DOUBLE_EQ(similarity, 1.);
}

TEST(ImageTools, computeSimilarity2Diff)
{
    cv::Mat left(100, 200, CV_8U, cv::Scalar((short)0));
    cv::Mat right(100, 200, CV_8U, cv::Scalar((short)255));
    double similarity = slideio::ImageTools::computeSimilarity2(left, right);
    EXPECT_DOUBLE_EQ(similarity, 0);
}
