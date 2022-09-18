#include <gtest/gtest.h>
#include "slideio/drivers/ndpi/ndpitifftools.hpp"
#include "tests/testlib/testtools.hpp"
#include <string>
#include <opencv2/highgui.hpp>

TEST(NDPITiffTools, scanFile)
{
    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "2017-02-27 15.29.08.ndpi");
    std::vector<slideio::NDPITiffDirectory> dirs;
    slideio::NDPITiffTools::scanFile(filePath, dirs);
    int dirCount = (int)dirs.size();
    ASSERT_EQ(dirCount, 5);
    const slideio::NDPITiffDirectory& dir = dirs[0];
    EXPECT_EQ(dir.width, 11520);
    EXPECT_EQ(dir.height, 9984);
    EXPECT_FALSE(dir.tiled);
    EXPECT_EQ(dir.rowsPerStrip, 9984);
    EXPECT_EQ(dir.channels, 3);
    EXPECT_EQ(dir.bitsPerSample, 8);
    EXPECT_EQ(dir.description.size(), 0);
    EXPECT_NE(dir.userLabel.size(), 0);
    EXPECT_EQ(dir.dataType, slideio::DataType::DT_Byte);
    //std::cout << dir.userLabel;
    const slideio::NDPITiffDirectory& dir5 = dirs[4];
    //std::cout << std::endl << std::endl << "------------------------" << std::endl << dir5.userLabel;
    EXPECT_EQ(dir5.width, 599);
    EXPECT_EQ(dir5.height, 204);
    EXPECT_FALSE(dir5.tiled);
    EXPECT_EQ(dir5.tileWidth, 0);
    EXPECT_EQ(dir5.tileHeight, 0);
    EXPECT_EQ(dir5.channels, 0);
    EXPECT_EQ(dir5.bitsPerSample, 8);
    EXPECT_EQ(dir5.description.size(), 0);
    EXPECT_TRUE(dir5.interleaved);
    EXPECT_EQ(0, dir5.res.x);
    EXPECT_EQ(0, dir5.res.y);
    EXPECT_EQ((uint32_t)1, dir5.compression);
}

TEST(NDPITiffTools, readRegularStripedDir)
{
    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-1.ndpi");
    std::string testFilePath = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-1.bin");
    std::vector<slideio::NDPITiffDirectory> dirs;
    slideio::NDPITiffTools::scanFile(filePath, dirs);
    int dirCount = (int)dirs.size();
    libtiff::TIFF* tiff = slideio::NDPITiffTools::openTiffFile(filePath);;
    ASSERT_TRUE(tiff != nullptr);
    int dirIndex = 3;
    slideio::NDPITiffDirectory dir;
    slideio::NDPITiffTools::scanTiffDirTags(tiff, dirIndex, 0, dir);
    dir.dataType = slideio::DataType::DT_Byte;
    cv::Mat dirRaster;
    slideio::NDPITiffTools::readStripedDir(tiff, dir, dirRaster);
    slideio::NDPITiffTools::closeTiffFile(tiff);
    EXPECT_EQ(dirRaster.rows, dir.height);
    EXPECT_EQ(dirRaster.cols, dir.width);
    // cv::FileStorage file(testFilePath, cv::FileStorage::WRITE);
    // file << "test" << dirRaster;
    // file.release();
    cv::Mat testRaster;
    cv::FileStorage file2(testFilePath, cv::FileStorage::READ);
    file2["test"] >> testRaster;
    cv::Mat diff = dirRaster != testRaster;
    // Equal if no elements disagree
    double min(1.), max(1.);
    cv::minMaxLoc(diff, &min, &max);
    EXPECT_EQ(min,0);
    EXPECT_EQ(max, 0);
}

