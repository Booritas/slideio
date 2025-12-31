#include <gtest/gtest.h>
//#include <opencv2/highgui.hpp>

#include "slideio/imagetools/tifftools.hpp"
#include "tests/testlib/testtools.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "opencv2/imgproc.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"

class TiffToolsTests : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        slideio::ImageDriverManager::setLogLevel("ERROR");
    }
    static void TearDownTestSuite() {
    }
};


TEST_F(TiffToolsTests, scanTiffFile)
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

TEST_F(TiffToolsTests, readStripedDir8bitInterleavedPhotometric2)
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
    slideio::ImageTools::readBitmap(filePathBmp, image);
	double sim = slideio::ImageTools::computeSimilarity(dirRaster, image);
	ASSERT_LT(0.99, sim);
}

TEST_F(TiffToolsTests, readStripedDir16bitSingleChannel)
{
    std::string filePathTiff = TestTools::getTestImagePath("gdal", "img_2448x2448_1x16bit_SRC_RGB_ducks.tif");
    std::string filePathRaw = TestTools::getTestImagePath("gdal", "img_2448x2448_1x16bit_SRC_RGB_ducks.raw");
    libtiff::TIFF* tiff = slideio::TiffTools::openTiffFile(filePathTiff);;
    ASSERT_TRUE(tiff != nullptr);
    int dirIndex = 0;
    slideio::TiffDirectory dir;
    slideio::TiffTools::scanTiffDirTags(tiff, dirIndex, 0, dir);
    cv::Mat dirRaster;
    slideio::TiffTools::readStripedDir(tiff, dir, dirRaster);
    slideio::TiffTools::closeTiffFile(tiff);
    cv::Mat rawRaster(dirRaster.rows, dirRaster.cols, dirRaster.type());
    TestTools::readRawImage(filePathRaw, rawRaster);
    double sim = slideio::ImageTools::computeSimilarity(dirRaster, rawRaster);
    ASSERT_LT(0.99, sim);
}

TEST_F(TiffToolsTests, readStripedDir16bitSingleChannel2ndDir)
{
    std::string filePathTiff = TestTools::getTestImagePath("gdal", "multipage-ducks.tif");
    std::string filePathRaw = TestTools::getTestImagePath("gdal", "img_2448x2448_1x16bit_SRC_RGB_ducks.raw");
    libtiff::TIFF* tiff = slideio::TiffTools::openTiffFile(filePathTiff);;
    ASSERT_TRUE(tiff != nullptr);
    int dirIndex = 1;
    slideio::TiffDirectory dir;
    slideio::TiffTools::scanTiffDirTags(tiff, dirIndex, 0, dir);
    cv::Mat dirRaster;
    slideio::TiffTools::readStripedDir(tiff, dir, dirRaster);
    slideio::TiffTools::closeTiffFile(tiff);
    cv::Mat rawRaster(dirRaster.rows, dirRaster.cols, dirRaster.type());
    TestTools::readRawImage(filePathRaw, rawRaster);
    double sim = slideio::ImageTools::computeSimilarity(dirRaster, rawRaster);
    ASSERT_LT(0.99, sim);
}

TEST_F(TiffToolsTests, readStripedDir16bit3Channels)
{
    std::string filePathTiff = TestTools::getTestImagePath("gdal", "img_2448x2448_3x16bit_SRC_RGB_ducks.tif");
    std::string filePathRaw = TestTools::getTestImagePath("gdal", "img_2448x2448_3x16bit_SRC_RGB_ducks.raw");
    libtiff::TIFF* tiff = slideio::TiffTools::openTiffFile(filePathTiff);;
    ASSERT_TRUE(tiff != nullptr);
    int dirIndex = 0;
    slideio::TiffDirectory dir;
    slideio::TiffTools::scanTiffDirTags(tiff, dirIndex, 0, dir);
    cv::Mat dirRaster;
    slideio::TiffTools::readStripedDir(tiff, dir, dirRaster);
    slideio::TiffTools::closeTiffFile(tiff);
    cv::Mat rawRaster(dirRaster.rows, dirRaster.cols, dirRaster.type());
    TestTools::readRawImage(filePathRaw, rawRaster);
    double sim = slideio::ImageTools::computeSimilarity(dirRaster, rawRaster);
    ASSERT_LT(0.99, sim);
}

TEST_F(TiffToolsTests, readTile_jpeg)
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

TEST_F(TiffToolsTests, readTile_J2K)
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

TEST_F(TiffToolsTests, readTile_jpeg_swapChannles)
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

TEST_F(TiffToolsTests, readPhotometricYCbCr)
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

TEST_F(TiffToolsTests, readNotRGBTile)
{
    std::string filePath = TestTools::getTestImagePath("scn", "Leica-Fluorescence-1.scn");
    std::vector<slideio::TiffDirectory> dirs;
    slideio::TiffTools::scanFile(filePath, dirs);
    int dirCount = (int)dirs.size();
    ASSERT_EQ(dirCount, 18);
    const slideio::TiffDirectory& dir = dirs[0];
    slideio::TIFFKeeper tiff(slideio::TiffTools::openTiffFile(filePath));

    std::string tilePath = TestTools::getTestImagePath("scn", "Leica-Fluorescence-1/tile.png");
    cv::Mat tile;
    slideio::ImageTools::readGDALImage(tilePath, tile);

    {
        cv::Mat raster;
        std::vector<int> channels = { 0,1,2 };
        slideio::TiffTools::readNotRGBTile(tiff, dir, 24, channels, raster);
        ASSERT_EQ(raster.cols, 512);
        ASSERT_EQ(raster.rows, 512);
        ASSERT_EQ(raster.channels(), 3);
        const int compare = std::memcmp(raster.data, tile.data, raster.total() * raster.elemSize());
        EXPECT_EQ(0, compare);
    }

    {
        cv::Mat raster;
        std::vector<int> channels = {};
        slideio::TiffTools::readNotRGBTile(tiff, dir, 24, channels, raster);
        ASSERT_EQ(raster.cols, 512);
        ASSERT_EQ(raster.rows, 512);
        ASSERT_EQ(raster.channels(), 3);
        const int compare = std::memcmp(raster.data, tile.data, raster.total() * raster.elemSize());
        EXPECT_EQ(0, compare);
    }

    {
        const int channelIndex = 1;
        cv::Mat tileChannel;
        cv::extractChannel(tile, tileChannel, channelIndex);
        cv::Mat raster;
        std::vector<int> channels = { channelIndex };
        slideio::TiffTools::readNotRGBTile(tiff, dir, 24, channels, raster);
        ASSERT_EQ(raster.cols, 512);
        ASSERT_EQ(raster.rows, 512);
        ASSERT_EQ(raster.channels(), 1);
        const int compare = std::memcmp(raster.data, tileChannel.data, raster.total() * raster.elemSize());
        EXPECT_EQ(0, compare);
    }
}

TEST_F(TiffToolsTests, openFileUtf8)
{
    std::string filePath = TestTools::getTestImagePath("gdal", u8"тест/тест.tif");
    std::vector<slideio::TiffDirectory> dirs;
    slideio::TiffTools::scanFile(filePath, dirs);
    int dirCount = (int)dirs.size();
    ASSERT_EQ(dirCount, 1);
}

