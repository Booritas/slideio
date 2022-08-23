#include <gtest/gtest.h>
#include <opencv2/highgui.hpp>

#include "slideio/core/imagetools/tifftools.hpp"
#include "testtools.hpp"
#include "slideio/core/imagetools/imagetools.hpp"
#include "opencv2/imgproc.hpp"


TEST(TiffTools, scanTiffFile)
{
    std::string filePath = TestTools::getTestImagePath("svs","JP2K-33003-1.svs");
    std::vector<slideio::TiffDirectory> dirs;
    slideio::TiffTools::scanFile(filePath, dirs);
    int dirCount = (int)dirs.size();
    ASSERT_EQ(dirCount, 6);
    const slideio::TiffDirectory& dir = dirs[0];
    EXPECT_EQ(dir.width, 15374);
    EXPECT_EQ(dir.height, 17497);
    EXPECT_TRUE(dir.tiled);
    EXPECT_EQ(dir.tileWidth, 256);
    EXPECT_EQ(dir.tileHeight, 256);
    EXPECT_EQ(dir.channels, 3);
    EXPECT_EQ(dir.bitsPerSample, 8);
    EXPECT_EQ(dir.description.size(),530);
    const slideio::TiffDirectory& dir5 = dirs[5];
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

TEST(TiffTools, readStripedDir)
{
    std::string filePathTiff = TestTools::getTestImagePath("svs","CMU-1-Small-Region.svs");
    std::string filePathBmp = TestTools::getTestImagePath("svs", "CMU-1-Small-Region-page-2.bmp");
    libtiff::TIFF* tiff = slideio::TiffTools::openTiffFile(filePathTiff);;
    ASSERT_TRUE(tiff!=nullptr);
    int dirIndex = 2;
    slideio::TiffDirectory dir;
    slideio::TiffTools::scanTiffDirTags(tiff, dirIndex, 0, dir);
    dir.dataType = slideio::DataType::DT_Byte;
    cv::Mat dirRaster;
    slideio::TiffTools::readStripedDir(tiff, dir, dirRaster);
    slideio::TiffTools::closeTiffFile(tiff);
    cv::Mat image;
    slideio::ImageTools::readGDALImage(filePathBmp, image);
    // compare similarity of rasters from bmp and decoded jp2k file
    cv::Mat score;
    cv::matchTemplate(dirRaster, image, score, cv::TM_CCOEFF_NORMED);
    double minScore(0), maxScore(0);
    cv::minMaxLoc(score, &minScore, &maxScore);
    ASSERT_LT(0.99, minScore);

}

TEST(TiffTools, readTile_jpeg)
{
    const std::string filePath = 
        TestTools::getTestImagePath("svs","CMU-1-Small-Region.svs");
    const std::string tilePath = 
        TestTools::getTestImagePath("svs","CMU-1-Small-Region-page-0-tile_5-5.bmp");
    // read tile from a tiff file
    libtiff::TIFF* tiff = slideio::TiffTools::openTiffFile(filePath);
    ASSERT_TRUE(tiff!=nullptr);
    slideio::TiffDirectory dir;
    slideio::TiffTools::scanTiffDir(tiff, 0, 0, dir);
    dir.dataType = slideio::DataType::DT_Byte;
    int tile_sx = (dir.width-1)/dir.tileWidth + 1;
    int tile = 5*tile_sx + 5;
    std::vector<int> channelIndices = {0};
    cv::Mat tileRaster;
    slideio::TiffTools::readTile(tiff, dir, tile, channelIndices, tileRaster);
    slideio::TiffTools::closeTiffFile(tiff);
    // read extracted tile
    cv::Mat bmpRaster;
    slideio::ImageTools::readGDALImage(tilePath, bmpRaster);
    cv::Mat bmpChannel;
    cv::extractChannel(bmpRaster, bmpChannel, 0);
    // compare similarity of rasters from bmp and decoded jp2k file
    auto dataSize = bmpChannel.total() * bmpChannel.elemSize();
    bool equal = std::equal(tileRaster.data, tileRaster.data + dataSize, bmpChannel.data);
    ASSERT_TRUE(equal);
}

TEST(TiffTools, readTile_J2K)
{
    const std::string filePath = 
        TestTools::getTestImagePath("svs","CMU-1-Small-Region.svs");
    const std::string bmpPath = 
        TestTools::getTestImagePath("svs","CMU-1-Small-Region-page-0-tile_5-5.bmp");
    // read tile from a tiff file
    libtiff::TIFF* tiff = slideio::TiffTools::openTiffFile(filePath);
    ASSERT_TRUE(tiff!=nullptr);
    slideio::TiffDirectory dir;
    slideio::TiffTools::scanTiffDir(tiff, 0,0,dir);
    dir.dataType = slideio::DataType::DT_Byte;
    int tile_sx = (dir.width-1)/dir.tileWidth + 1;
    int tile = 5*tile_sx + 5;
    cv::Mat tileRaster;
    std::vector<int> channelIndices;
    slideio::TiffTools::readTile(tiff, dir, tile, channelIndices, tileRaster);
    slideio::TiffTools::closeTiffFile(tiff);
    cv::Mat bmpRaster;
    slideio::ImageTools::readGDALImage(bmpPath, bmpRaster);
    // compare similarity of rasters from bmp and decoded jp2k file
    cv::Mat score;
    cv::matchTemplate(tileRaster, bmpRaster, score, cv::TM_CCOEFF_NORMED);
    double minScore(0), maxScore(0);
    cv::minMaxLoc(score, &minScore, &maxScore);
    ASSERT_LT(0.99, minScore);
}

TEST(TiffTools, readTile_jpeg_swapChannles)
{
    const std::string filePath =
        TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs");
    const std::string tilePath =
        TestTools::getTestImagePath("svs", "CMU-1-Small-Region-page-0-tile_5-5.bmp");
    // read tile from a tiff file
    libtiff::TIFF* tiff = slideio::TiffTools::openTiffFile(filePath);
    ASSERT_TRUE(tiff != nullptr);
    slideio::TiffDirectory dir;
    slideio::TiffTools::scanTiffDir(tiff, 0, 0, dir);
    dir.dataType = slideio::DataType::DT_Byte;
    int tile_sx = (dir.width - 1) / dir.tileWidth + 1;
    int tile = 5 * tile_sx + 5;
    std::vector<int> channelIndices = { 2, 1, 0 };
    cv::Mat tileRaster;
    slideio::TiffTools::readTile(tiff, dir, tile, channelIndices, tileRaster);
    slideio::TiffTools::closeTiffFile(tiff);
    // read extracted tile
    cv::Mat bmpRaster;
    slideio::ImageTools::readGDALImage(tilePath, bmpRaster);
    ASSERT_EQ(bmpRaster.cols, tileRaster.cols);
    ASSERT_EQ(bmpRaster.rows, tileRaster.rows);
    ASSERT_EQ(bmpRaster.channels(), tileRaster.channels());

    int channelSize = bmpRaster.cols * bmpRaster.rows;

    for (int swapedIndex = 0; swapedIndex < 3; ++swapedIndex)
    {
        int originIndex = channelIndices[swapedIndex];
        cv::Mat originChannel, swapedChannel;
        cv::extractChannel(bmpRaster, originChannel, originIndex);
        cv::extractChannel(tileRaster, swapedChannel, swapedIndex);
        EXPECT_EQ(std::memcmp(originChannel.data, swapedChannel.data, channelSize), 0);
    }
}

TEST(TiffTools, readPhotometricYCbCr)
{
    std::string filePath = TestTools::getTestImagePath("scn", "Leica-Fluorescence-1.scn");
    std::vector<slideio::TiffDirectory> dirs;
    slideio::TiffTools::scanFile(filePath, dirs);
    int dirCount = (int)dirs.size();
    ASSERT_EQ(dirCount, 18);
    const slideio::TiffDirectory& dir = dirs[0];
    EXPECT_EQ(dir.photometric, 6);
    EXPECT_EQ(dir.YCbCrSubsampling[0], 2);
    EXPECT_EQ(dir.YCbCrSubsampling[1], 2);
    const slideio::TiffDirectory& dir2 = dirs[6];
    EXPECT_EQ(dir2.photometric, 1);
    EXPECT_EQ(dir2.YCbCrSubsampling[0], 2);
    EXPECT_EQ(dir2.YCbCrSubsampling[1], 2);
}

TEST(TiffTools, readNotRGBTile)
{
    std::string filePath = TestTools::getTestImagePath("scn", "Leica-Fluorescence-1.scn");
    std::vector<slideio::TiffDirectory> dirs;
    slideio::TiffTools::scanFile(filePath, dirs);
    int dirCount = (int)dirs.size();
    ASSERT_EQ(dirCount, 18);
    const slideio::TiffDirectory& dir = dirs[0];
    slideio::TIFFKeeper tiff(slideio::TiffTools::openTiffFile(filePath));
    std::vector<int> channels = { 0,1,2 };
    cv::Mat raster;
    slideio::TiffTools::readNotRGBTile(tiff, dir, 24, channels, raster);
    ASSERT_EQ(raster.cols, 512);
    ASSERT_EQ(raster.rows, 512);
    ASSERT_EQ(raster.channels(), 3);
    std::string tilePath = TestTools::getTestImagePath("scn", "Leica-Fluorescence-1/tile.png");
    cv::Mat tile;
    slideio::ImageTools::readGDALImage(tilePath, tile);
    const int compare = std::memcmp(raster.data, tile.data, raster.total() * raster.elemSize());
    EXPECT_EQ(compare, 0);
}