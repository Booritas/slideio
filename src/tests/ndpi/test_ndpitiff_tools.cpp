#include <gtest/gtest.h>
#include "slideio/drivers/ndpi/ndpitifftools.hpp"
#include "tests/testlib/testtools.hpp"
#include <string>
#include <opencv2/imgproc.hpp>

#include "slideio/core/tools/cachemanager.hpp"
#include "slideio/drivers/ndpi/ndpifile.hpp"
#include "slideio/imagetools/imagetools.hpp"


TEST(NDPITiffTools, scanFile)
{
    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "2017-02-27 15.29.08.ndpi");
    slideio::NDPIFile file;
    file.init(filePath);
    const std::vector<slideio::NDPITiffDirectory>& dirs = file.directories();
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
    EXPECT_EQ(dir.userLabel.size(), 0);
    EXPECT_NE(dir.comments.size(), 0);
    EXPECT_EQ(dir.dataType, slideio::DataType::DT_Byte);
    EXPECT_EQ(dir.getType(), slideio::NDPITiffDirectory::Type::SingleStripeMCU);
    const slideio::NDPITiffDirectory& dir5 = dirs[4];
    EXPECT_EQ(dir5.width, 599);
    EXPECT_EQ(dir5.height, 204);
    EXPECT_FALSE(dir5.tiled);
    EXPECT_EQ(dir5.tileWidth, 0);
    EXPECT_EQ(dir5.tileHeight, 0);
    EXPECT_EQ(dir5.channels, 1);
    EXPECT_EQ(dir5.bitsPerSample, 8);
    EXPECT_EQ(dir5.description.size(), 0);
    EXPECT_TRUE(dir5.interleaved);
    EXPECT_DOUBLE_EQ(0.00012820512820512821, dir5.res.x);
    EXPECT_DOUBLE_EQ(0.00012820512820512821, dir5.res.y);
    EXPECT_EQ((uint32_t)1, dir5.compression);
    EXPECT_EQ(dir5.getType(), slideio::NDPITiffDirectory::Type::SingleStripe);
}

TEST(NDPITiffTools, readRegularStripedDir)
{
    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-1.ndpi");
    std::string testFilePath = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-1-dir.png");
    slideio::NDPIFile file;
    file.init(filePath);
    const std::vector<slideio::NDPITiffDirectory>& dirs = file.directories();
    int dirCount = (int)dirs.size();
    int dirIndex = 3;
    cv::Mat dirRaster;
    auto tiff = slideio::NDPITiffTools::openTiffFile(filePath);
    auto& dir = dirs[dirIndex];
    slideio::NDPITiffTools::readStripedDir(tiff, dir, dirRaster);
    slideio::NDPITiffTools::closeTiffFile(tiff);
    EXPECT_EQ(dirRaster.rows, dir.height);
    EXPECT_EQ(dirRaster.cols, dir.width);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    cv::cvtColor(testRaster, testRaster, cv::COLOR_BGRA2RGB);
    cv::Mat diff = dirRaster != testRaster;
    //TestTools::showRaster(dirRaster);
    double sim = slideio::ImageTools::computeSimilarity2(dirRaster, testRaster);
    EXPECT_GE(sim, 0.99);
}

TEST(NDPITiffTools, readRegularStrip)
{
    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-1.ndpi");
    std::string testFilePath = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-1-dir.png");
    libtiff::TIFF* tiff = slideio::NDPITiffTools::openTiffFile(filePath);;
    ASSERT_TRUE(tiff != nullptr);
    int dirIndex = 3;
    slideio::NDPITiffDirectory dir;
    slideio::NDPITiffTools::scanTiffDirTags(tiff, dirIndex, 0, dir);
    dir.dataType = slideio::DataType::DT_Byte;
    const std::vector<int> channelIndices = { 0,1,2 };
    cv::Mat stripRaster;
    slideio::NDPITiffTools::scanTiffDirTags(tiff, 0, 0, dir);
    slideio::NDPITiffTools::scanTiffDirTags(tiff, dirIndex, 0, dir);
    slideio::NDPITiffTools::readStripe(tiff, dir, 0, channelIndices, stripRaster);
    slideio::NDPITiffTools::closeTiffFile(tiff);
    EXPECT_EQ(stripRaster.rows, dir.rowsPerStrip);
    EXPECT_EQ(stripRaster.cols, dir.width);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    cv::cvtColor(testRaster, testRaster, cv::COLOR_BGRA2RGB);
    double similarity = slideio::ImageTools::computeSimilarity2(stripRaster, testRaster);
    //TestTools::showRaster(stripRaster);
    //TestTools::showRaster(testRaster);
    EXPECT_GT(similarity, 0.99);
}


TEST(NDPITiffTools, readTile)
{
    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "DM0014 - 2020-04-02 10.25.21.ndpi");
    std::string testFilePath = TestTools::getFullTestImagePath("hamamatsu", "DM0014 - 2020-04-02 10.25.21-tile.png");
    slideio::NDPIFile file;
    file.init(filePath);
    const std::vector<slideio::NDPITiffDirectory>& dirs = file.directories();
    int dirCount = (int)dirs.size();

    libtiff::TIFF* tiff = slideio::NDPITiffTools::openTiffFile(filePath);;
    ASSERT_TRUE(tiff != nullptr);
    const int dirIndex = 3;
    const int tileIndex = 306;
    cv::Mat tileRaster;
    const slideio::NDPITiffDirectory& dir = dirs[dirIndex];
    const std::vector<int> channelIndices = { 0,1,2 };
    slideio::NDPITiffTools::readTile(tiff, dir, tileIndex, channelIndices, tileRaster);
    slideio::NDPITiffTools::closeTiffFile(tiff);
    EXPECT_EQ(tileRaster.rows, dir.tileHeight);
    EXPECT_EQ(tileRaster.cols, dir.tileWidth);
    // TestTools::showRaster(tileRaster);
    // TestTools::writePNG(tileRaster, testFilePath);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    TestTools::compareRasters(tileRaster, testRaster);
}

TEST(NDPITiffTools, compupteTileCounts)
{
    slideio::NDPITiffDirectory dir;
    dir.width = 1000;
    dir.height = 2000;
    dir.tileHeight = 100;
    dir.tileWidth = 100;
    cv::Size counts = slideio::NDPITiffTools::computeTileCounts(dir);
    EXPECT_EQ(10, counts.width);
    EXPECT_EQ(20, counts.height);
    dir.width = 999;
    dir.height = 1960;
    counts = slideio::NDPITiffTools::computeTileCounts(dir);
    EXPECT_EQ(10, counts.width);
    EXPECT_EQ(20, counts.height);
    dir.width = 50;
    dir.height = 50;
    counts = slideio::NDPITiffTools::computeTileCounts(dir);
    EXPECT_EQ(1, counts.width);
    EXPECT_EQ(1, counts.height);
    dir.width = 101;
    dir.height = 50;
    counts = slideio::NDPITiffTools::computeTileCounts(dir);
    EXPECT_EQ(2, counts.width);
    EXPECT_EQ(1, counts.height);
    dir.width = 50;
    dir.height = 101;
    counts = slideio::NDPITiffTools::computeTileCounts(dir);
    EXPECT_EQ(1, counts.width);
    EXPECT_EQ(2, counts.height);
}

TEST(NDPITiffTools, compupteTileSize)
{
    slideio::NDPITiffDirectory dir;
    dir.width = 1000;
    dir.height = 1500;
    dir.tileHeight = 100;
    dir.tileWidth = 150;
    cv::Size size = slideio::NDPITiffTools::computeTileSize(dir, 0);
    EXPECT_EQ(150, size.width);
    EXPECT_EQ(100, size.height);

    dir.width = 99;
    dir.height = 85;
    dir.tileHeight = 100;
    dir.tileWidth = 150;
    size = slideio::NDPITiffTools::computeTileSize(dir, 0);
    EXPECT_EQ(99, size.width);
    EXPECT_EQ(85, size.height);

    dir.width = 101;
    dir.height = 151;
    dir.tileHeight = 100;
    dir.tileWidth = 150;
    EXPECT_ANY_THROW(slideio::NDPITiffTools::computeTileSize(dir, 3));

    dir.height = 151;
    dir.width = 101;
    dir.tileHeight = 150;
    dir.tileWidth = 100;
    size = slideio::NDPITiffTools::computeTileSize(dir, 3);
    EXPECT_EQ(1, size.width);
    EXPECT_EQ(1, size.height);

    dir.height = 151;
    dir.width = 301;
    dir.tileHeight = 150;
    dir.tileWidth = 100;
    size = slideio::NDPITiffTools::computeTileSize(dir, 5);
    EXPECT_EQ(100, size.width);
    EXPECT_EQ(1, size.height);
}

TEST(NDPITiffTools, computeStripHeight)
{
    slideio::NDPITiffDirectory dir;
    dir.width = 1000;
    dir.height = 1500;
    dir.rowsPerStrip = 100;
    int lines = slideio::NDPITiffTools::computeStripHeight(dir.height,dir.rowsPerStrip, 0);
    EXPECT_EQ(100, lines);
    dir.width = 1000;
    dir.height = 101;
    dir.rowsPerStrip = 100;
    lines = slideio::NDPITiffTools::computeStripHeight(dir.height, dir.rowsPerStrip, 1);
    EXPECT_EQ(1, lines);
}


TEST(NDPITiffTools, readScanlines)
{
    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-1.ndpi");
    std::string testFilePath = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-1-scanline.png");
    libtiff::TIFF* tiff = slideio::NDPITiffTools::openTiffFile(filePath);;
    ASSERT_TRUE(tiff != nullptr);
    int dirIndex = 3;
    slideio::NDPITiffDirectory dir;
    slideio::NDPITiffTools::scanTiffDirTags(tiff, dirIndex, 0, dir);
    dir.dataType = slideio::DataType::DT_Byte;
    const std::vector<int> channelIndices = { 0,1,2 };
    cv::Mat stripRaster;
    slideio::NDPITiffTools::scanTiffDirTags(tiff, 0, 0, dir);
    slideio::NDPITiffTools::scanTiffDirTags(tiff, dirIndex, 0, dir);
    const int numberScanlines = 300;
    const int firstScanline = 200;
    FILE* file = fopen(filePath.c_str(), "rb");
    slideio::NDPITiffTools::readJpegScanlines(tiff, file, dir, firstScanline, numberScanlines, channelIndices, stripRaster);
    slideio::NDPITiffTools::closeTiffFile(tiff);
    fclose(file);
    EXPECT_EQ(numberScanlines, stripRaster.rows);
    EXPECT_EQ(dir.width, stripRaster.cols);
    //slideio::NDPITestTools::writePNG(stripRaster, testFilePath);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    cv::cvtColor(testRaster, testRaster, cv::COLOR_BGRA2BGR);
    TestTools::compareRasters(testRaster, stripRaster);
    //showRaster(stripRaster);
}


TEST(NDPITiffTools, readRegularStripedDir2)
{
    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "2017-02-27 15.29.08.ndpi");
    std::string testFilePath = TestTools::getFullTestImagePath("hamamatsu", "2017-02-27 15.29.08-2.png");
    slideio::NDPIFile file;
    file.init(filePath);
    const std::vector<slideio::NDPITiffDirectory>& dirs = file.directories();
    int dirCount = (int)dirs.size();
    libtiff::TIFF* tiff = slideio::NDPITiffTools::openTiffFile(filePath);;
    ASSERT_TRUE(tiff != nullptr);
    int dirIndex = 2;
    slideio::NDPITiffDirectory dir;
    slideio::NDPITiffTools::scanTiffDirTags(tiff, dirIndex, 0, dir);
    dir.dataType = slideio::DataType::DT_Byte;
    cv::Mat dirRaster;
    slideio::NDPITiffTools::readStripedDir(tiff, dir, dirRaster);
    slideio::NDPITiffTools::closeTiffFile(tiff);
    EXPECT_EQ(dirRaster.rows, dir.height);
    EXPECT_EQ(dirRaster.cols, dir.width);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    cv::cvtColor(testRaster, testRaster, cv::COLOR_BGRA2RGB);
    double similarity = slideio::ImageTools::computeSimilarity2(dirRaster, testRaster);
    EXPECT_GT(similarity, 0.98);
}


TEST(NDPITiffTools, readScanlinesDNLMarker)
{
    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "HE_Hamamatsu.ndpi");
    std::string testFilePath = TestTools::getFullTestImagePath("hamamatsu", "HE_Hamamatsu-roi.png");
    libtiff::TIFF* tiff = slideio::NDPITiffTools::openTiffFile(filePath);
    ASSERT_TRUE(tiff != nullptr);
    int dirIndex = 0;
    slideio::NDPITiffDirectory dir;
    slideio::NDPITiffTools::scanTiffDirTags(tiff, dirIndex, 0, dir);
    dir.dataType = slideio::DataType::DT_Byte;
    const std::vector<int> channelIndices = { 0,1,2 };
    cv::Mat stripRaster;
    cv::Rect roi = { dir.width / 2, dir.height / 2, 400, 300 };
    slideio::NDPITiffTools::readJpegDirectoryRegion(tiff, filePath, roi, dir, channelIndices, stripRaster);
    EXPECT_EQ(roi.height, stripRaster.rows);
    EXPECT_EQ(roi.width, stripRaster.cols);
    //slideio::NDPITestTools::writePNG(stripRaster, testFilePath);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    TestTools::compareRasters(testRaster, stripRaster);
    //showRaster(stripRaster);
}

TEST(NDPITiffTools, readScanlinesDNLMarkerSingleChannel)
{
    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "HE_Hamamatsu.ndpi");
    std::string testFilePath = TestTools::getFullTestImagePath("hamamatsu", "HE_Hamamatsu-roi-gray.png");
    libtiff::TIFF* tiff = slideio::NDPITiffTools::openTiffFile(filePath);
    ASSERT_TRUE(tiff != nullptr);
    int dirIndex = 0;
    slideio::NDPITiffDirectory dir;
    slideio::NDPITiffTools::scanTiffDirTags(tiff, dirIndex, 0, dir);
    dir.dataType = slideio::DataType::DT_Byte;
    const std::vector<int> channelIndices = { 1 };
    cv::Mat stripRaster;
    cv::Rect roi = { dir.width / 3, dir.height / 3, 400, 300 };
    slideio::NDPITiffTools::readJpegDirectoryRegion(tiff, filePath, roi, dir, channelIndices, stripRaster);
    EXPECT_EQ(roi.height, stripRaster.rows);
    EXPECT_EQ(roi.width, stripRaster.cols);
    EXPECT_EQ(1, stripRaster.channels());
    //slideio::NDPITestTools::writePNG(stripRaster, testFilePath);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    TestTools::compareRasters(testRaster, stripRaster);
    //showRaster(stripRaster);
}

TEST(NDPITiffTools, readScanlinesDNLMarkerInversedChannels)
{
    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "HE_Hamamatsu.ndpi");
    std::string testFilePath = TestTools::getFullTestImagePath("hamamatsu", "HE_Hamamatsu-roi-inversed.png");
    libtiff::TIFF* tiff = slideio::NDPITiffTools::openTiffFile(filePath);
    ASSERT_TRUE(tiff != nullptr);
    int dirIndex = 0;
    slideio::NDPITiffDirectory dir;
    slideio::NDPITiffTools::scanTiffDirTags(tiff, dirIndex, 0, dir);
    dir.dataType = slideio::DataType::DT_Byte;
    const std::vector<int> channelIndices = { 2,0,1};
    cv::Mat stripRaster;
    cv::Rect roi = { dir.width / 2, dir.height / 2, 400, 300 };
    slideio::NDPITiffTools::readJpegDirectoryRegion(tiff, filePath, roi, dir, channelIndices, stripRaster);
    EXPECT_EQ(roi.height, stripRaster.rows);
    EXPECT_EQ(roi.width, stripRaster.cols);
    //slideio::NDPITestTools::writePNG(stripRaster, testFilePath);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    TestTools::compareRasters(testRaster, stripRaster);
    //showRaster(stripRaster);
}

TEST(NDPITiffTools, cacheScanlines)
{
    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-1.ndpi");
    std::string testFilePath = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-1-cacheLines.png");
    slideio::NDPIFile file;
    file.init(filePath);
    const std::vector<slideio::NDPITiffDirectory>& dirs = file.directories();
    const slideio::NDPITiffDirectory& dir = dirs[2];
    auto tiffFile = slideio::NDPITiffTools::openTiffFile(filePath);
    std::shared_ptr<slideio::CacheManager> cacheManager = std::make_shared<slideio::CacheManager>();
    const cv::Size tileSize = { 100, 200 };
    slideio::NDPITiffTools::cacheScanlines(&file, dir, tileSize, cacheManager.get());
    const int tileCountY = (dir.height - 1) / tileSize.height + 1;
    const int tileCountX = (dir.width - 1) / tileSize.width + 1;
    const int tileCount = tileCountX * tileCountY;
    ASSERT_EQ(cacheManager->getTileCount(dir.dirIndex) , tileCount);
    const cv::Rect& rect = cacheManager->getTileRect(dir.dirIndex, 0);
    EXPECT_EQ(rect, cv::Rect(cv::Point2i(0, 0), tileSize));
    slideio::CacheManagerTiler tiler(cacheManager, tileSize, dir.dirIndex);
    cv::Rect blockRect = { dir.width/2, dir.height/4, 750, 700 };
    cv::Size blockSize = blockRect.size();
    cv::Mat blockRaster(blockSize, CV_8UC3, cv::Scalar(0, 0, 0));
    slideio::TileComposer::composeRect(&tiler, {}, blockRect, blockSize, blockRaster);
    slideio::NDPITiffTools::closeTiffFile(tiffFile);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    TestTools::compareRasters(testRaster, blockRaster);
    //TestTools::showRaster(blockRaster);
    //TestTools::writePNG(blockRaster, testFilePath);
}


TEST(NDPITiffTools, readMCUTile)
{
    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-1.ndpi");
    std::string testFilePath = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-1-tile.png");
    slideio::NDPIFile ndpi;
    ndpi.init(filePath);
    size_t dirCount = ndpi.directories().size();
    const slideio::NDPITiffDirectory& dir = ndpi.directories()[0];
    cv::Mat tileRaster;
    std::unique_ptr<FILE, slideio::Tools::FileDeleter> sfile(slideio::Tools::openFile(ndpi.getFilePath(), "rb"));
    FILE* file = sfile.get();
    slideio::NDPITiffTools::readMCUTile(file, dir, 87501, tileRaster);
    EXPECT_EQ(tileRaster.rows, dir.tileHeight);
    EXPECT_EQ(tileRaster.cols, dir.tileWidth);
    EXPECT_EQ(tileRaster.channels(), 3);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    cv::cvtColor(testRaster, testRaster, cv::COLOR_BGRA2BGR);
    TestTools::compareRasters(tileRaster, testRaster);
}


TEST(NDPITiffTools, getDirectoryType) {

    slideio::NDPITiffDirectory dir;
    dir.width = 100;
    dir.height = 100;

    dir.tiled = true;
    EXPECT_EQ(dir.getType(), slideio::NDPITiffDirectory::Type::Tiled);

    dir.tiled = false;
    dir.tileWidth = 1;
    dir.tileHeight = 1;
    dir.mcuStarts.push_back(1);
    dir.slideioCompression = slideio::Compression::Jpeg;
    EXPECT_EQ(dir.getType(), slideio::NDPITiffDirectory::Type::SingleStripeMCU);

    dir.tiled = false;
    dir.tileWidth = 10;
    dir.tileHeight = 10;
    dir.rowsPerStrip = dir.height;
    dir.mcuStarts.push_back(1);
    dir.slideioCompression = slideio::Compression::Uncompressed;
    EXPECT_EQ(dir.getType(), slideio::NDPITiffDirectory::Type::SingleStripe);

    dir.tiled = false;
    dir.tileWidth = 10;
    dir.tileHeight = 10;
    dir.rowsPerStrip = dir.tileHeight;
    dir.mcuStarts.push_back(1);
    dir.slideioCompression = slideio::Compression::Uncompressed;
    EXPECT_EQ(dir.getType(), slideio::NDPITiffDirectory::Type::Striped);

    dir.tileWidth = 0;
    dir.tileHeight = 0;
    dir.mcuStarts.clear();
    dir.rowsPerStrip = dir.height;
    EXPECT_EQ(dir.getType(), slideio::NDPITiffDirectory::Type::SingleStripe);

    dir.rowsPerStrip = 0;
    EXPECT_EQ(dir.getType(), slideio::NDPITiffDirectory::Type::Striped);
}
