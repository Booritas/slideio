#include <gtest/gtest.h>
#include "slideio/imagetools/tifftools.hpp"
#include "testtools.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "opencv2/imgproc.hpp"

TEST(Slideio_TiffTools, scanTiffFile)
{
    std::string filePath = TestTools::getTestImagePath("svs","JP2K-33003-1.svs");
    std::vector<cv::slideio::TiffDirectory> dirs;
    cv::slideio::TiffTools::scanFile(filePath, dirs);
    int dirCount = (int)dirs.size();
    ASSERT_EQ(dirCount, 6);
    const cv::slideio::TiffDirectory& dir = dirs[0];
    EXPECT_EQ(dir.width, 15374);
    EXPECT_EQ(dir.height, 17497);
    EXPECT_TRUE(dir.tiled);
    EXPECT_EQ(dir.tileWidth, 256);
    EXPECT_EQ(dir.tileHeight, 256);
    EXPECT_EQ(dir.channels, 3);
    EXPECT_EQ(dir.bitsPerSample, 8);
    EXPECT_EQ(dir.description.size(),530);
    const cv::slideio::TiffDirectory& dir5 = dirs[5];
    EXPECT_EQ(dir5.width, 1280);
    EXPECT_EQ(dir5.height, 421);
    EXPECT_FALSE(dir5.tiled);
    EXPECT_EQ(dir5.tileWidth, 0);
    EXPECT_EQ(dir5.tileHeight, 0);
    EXPECT_EQ(dir5.channels, 3);
    EXPECT_EQ(dir5.bitsPerSample, 8);
    EXPECT_EQ(dir5.description.size(),44);
    EXPECT_TRUE(dir5.interleaved);
    EXPECT_EQ(0, dir5.res.x);
    EXPECT_EQ(0,dir5.res.y);
    EXPECT_EQ((uint32_t)7,dir5.compression);
}

TEST(Slideio_TiffTools, readStripedDir)
{
    std::string filePathTiff = TestTools::getTestImagePath("svs","CMU-1-Small-Region.svs");
    std::string filePathBmp = TestTools::getTestImagePath("svs", "CMU-1-Small-Region-page-2.bmp");
    TIFF* tiff = cv::slideio::TiffTools::openTiffFile(filePathTiff);;
    ASSERT_TRUE(tiff!=nullptr);
    int dirIndex = 2;
    cv::slideio::TiffDirectory dir;
    cv::slideio::TiffTools::scanTiffDirTags(tiff, dirIndex, 0, dir);
    dir.dataType = cv::slideio::DataType::DT_Byte;
    cv::Mat dirRaster;
    cv::slideio::TiffTools::readStripedDir(tiff, dir, dirRaster);
    cv::slideio::TiffTools::closeTiffFile(tiff);
    cv::Mat image;
    cv::slideio::ImageTools::readGDALImage(filePathBmp, image);
    // compare similarity of rasters from bmp and decoded jp2k file
    cv::Mat score;
    cv::matchTemplate(dirRaster, image, score, cv::TM_CCOEFF_NORMED);
    double minScore(0), maxScore(0);
    cv::minMaxLoc(score, &minScore, &maxScore);
    ASSERT_LT(0.99, minScore);

}

TEST(Slideio_TiffTools, readTile_jpeg)
{
    const std::string filePath = 
        TestTools::getTestImagePath("svs","CMU-1-Small-Region.svs");
    const std::string tilePath = 
        TestTools::getTestImagePath("svs","CMU-1-Small-Region-page-0-tile_5-5.bmp");
    // read tile from a tiff file
    TIFF* tiff = cv::slideio::TiffTools::openTiffFile(filePath);
    ASSERT_TRUE(tiff!=nullptr);
    cv::slideio::TiffDirectory dir;
    cv::slideio::TiffTools::scanTiffDir(tiff, 0, 0, dir);
    dir.dataType = cv::slideio::DataType::DT_Byte;
    int tile_sx = (dir.width-1)/dir.tileWidth + 1;
    int tile = 5*tile_sx + 5;
    std::vector<int> channelIndices = {0};
    cv::Mat tileRaster;
    cv::slideio::TiffTools::readTile(tiff, dir, tile, channelIndices, tileRaster);
    cv::slideio::TiffTools::closeTiffFile(tiff);
    // read extracted tile
    cv::Mat bmpRaster;
    cv::slideio::ImageTools::readGDALImage(tilePath, bmpRaster);
    cv::Mat bmpChannel;
    cv::extractChannel(bmpRaster, bmpChannel, 0);
    // compare similarity of rasters from bmp and decoded jp2k file
    auto dataSize = bmpChannel.total() * bmpChannel.elemSize();
    bool equal = std::equal(tileRaster.data, tileRaster.data + dataSize, bmpChannel.data);
    ASSERT_TRUE(equal);
}

TEST(Slideio_TiffTools, readTile_J2K)
{
    const std::string filePath = 
        TestTools::getTestImagePath("svs","CMU-1-Small-Region.svs");
    const std::string bmpPath = 
        TestTools::getTestImagePath("svs","CMU-1-Small-Region-page-0-tile_5-5.bmp");
    // read tile from a tiff file
    TIFF* tiff = cv::slideio::TiffTools::openTiffFile(filePath);
    ASSERT_TRUE(tiff!=nullptr);
    cv::slideio::TiffDirectory dir;
    cv::slideio::TiffTools::scanTiffDir(tiff, 0,0,dir);
    dir.dataType = cv::slideio::DataType::DT_Byte;
    int tile_sx = (dir.width-1)/dir.tileWidth + 1;
    int tile = 5*tile_sx + 5;
    cv::Mat tileRaster;
    std::vector<int> channelIndices;
    cv::slideio::TiffTools::readTile(tiff, dir, tile, channelIndices, tileRaster);
    cv::slideio::TiffTools::closeTiffFile(tiff);
    cv::Mat bmpRaster;
    cv::slideio::ImageTools::readGDALImage(bmpPath, bmpRaster);
    // compare similarity of rasters from bmp and decoded jp2k file
    cv::Mat score;
    cv::matchTemplate(tileRaster, bmpRaster, score, cv::TM_CCOEFF_NORMED);
    double minScore(0), maxScore(0);
    cv::minMaxLoc(score, &minScore, &maxScore);
    ASSERT_LT(0.99, minScore);
}
