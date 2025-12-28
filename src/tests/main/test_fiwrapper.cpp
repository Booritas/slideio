// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/base/exceptions.hpp"
#include "slideio/imagetools/fiwrapper.hpp"
#include "tests/testlib/testtools.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include <gtest/gtest.h>

using namespace slideio;

TEST(FIWrapper, constructorWithValidFile)
{
    std::string filePath = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    EXPECT_NO_THROW({
        FIWrapper wrapper(filePath);
	    EXPECT_TRUE(wrapper.isValid());
		EXPECT_EQ(1, wrapper.getNumPages());
    });
}

TEST(FIWrapper, constructorWithNonExistentFile)
{
    std::string filePath = TestTools::getTestImagePath("gdal", "non_existent_file.png");
    EXPECT_THROW({
        FIWrapper wrapper(filePath);
    }, RuntimeError);
}

TEST(FIWrapper, constructorWithInvalidFormat)
{
    std::string filePath = TestTools::getTestImagePath("czi", "08_18_2018_enc_1001_633.czi");
    EXPECT_THROW({
        FIWrapper wrapper(filePath);
    }, RuntimeError);
}


TEST(FIWrapper, openMultipleFiles)
{
    std::string filePath1 = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    std::string filePath2 = TestTools::getTestImagePath("gdal", "img_2448x2448_1x8bit_SRC_GRAY_ducks.png");
    
    EXPECT_NO_THROW({
        FIWrapper wrapper1(filePath1);
        FIWrapper wrapper2(filePath2);
        EXPECT_TRUE(wrapper1.isValid());
		EXPECT_TRUE(wrapper2.isValid());
        EXPECT_EQ(1, wrapper1.getNumPages());
        EXPECT_EQ(1, wrapper2.getNumPages());
        });
}


#if defined(WIN32)
TEST(FIWrapper, unicodeFilePath)
{
    std::string filePath = TestTools::getFullTestImagePath("unicode", u8"тест/lena_256.jpg");
    FIWrapper wrapper(filePath);
	EXPECT_TRUE(wrapper.isValid());
    EXPECT_EQ(1, wrapper.getNumPages());
    auto page = wrapper.readPage(0);
	EXPECT_TRUE(page != nullptr);
    Size size = page->getSize();
    EXPECT_EQ(256, size.width);
	EXPECT_EQ(256, size.height);
}
#endif


TEST(FIWrapper, emptyFilePath)
{
    std::string filePath = "";
    EXPECT_THROW({
        FIWrapper wrapper(filePath);
    }, std::exception);
}

namespace
{
    void testFICompression(const std::string& testFileName, Compression compression) {
        std::string filePath = TestTools::getTestImagePath("gdal", testFileName);
        std::string testFilePath = TestTools::getTestImagePath("gdal", "test.tif");
        FIWrapper wrapper(filePath);
        EXPECT_TRUE(wrapper.isValid());
        EXPECT_EQ(1, wrapper.getNumPages());
        auto page = wrapper.readPage(0);
        EXPECT_EQ(compression, page->getCompression());
        int numChannels = 4;
        if (compression != Compression::Png 
            && compression != Compression::Uncompressed 
            && compression != Compression::GIF) {
            numChannels = 3;
        }
        EXPECT_EQ(numChannels, page->getNumChannels());
        EXPECT_EQ(DataType::DT_Byte, page->getDataType());
        Size size = page->getSize();
        EXPECT_EQ(650, size.width);
        EXPECT_EQ(434, size.height);
        cv::Mat raster;
        page->readRaster(raster);
        EXPECT_EQ(size.height, raster.rows);
        EXPECT_EQ(size.width, raster.cols);
        EXPECT_EQ(page->getNumChannels(), raster.channels());
        const std::string& metadata = page->getMetadata();
        TIFFKeeper tiff = TiffTools::openTiffFile(testFilePath);
        std::vector<TiffDirectory> dirs;
        TiffTools::scanFile(tiff, dirs);
        cv::Mat testRaster;
        TiffTools::readStripedDir(tiff.getHandle(), dirs[0], testRaster);
        EXPECT_EQ(raster.size(), testRaster.size());
        if (raster.channels() == 3) {
            // remove last channel
			cv::Mat channels[4];
			cv::split(testRaster, channels);
			cv::Mat merged;
			cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, testRaster);
        }
        double sim = ImageTools::computeSimilarity2(raster, testRaster);
        EXPECT_GT(sim, 0.99);
    }
}

TEST(FIWrapper, openTiffFile)
{
	testFICompression("test.tif", Compression::Uncompressed);
}

TEST(FIWrapper, openTiffJpegFile)
{
    testFICompression("test-jpeg.tif", Compression::Jpeg);
}

TEST(FIWrapper, openJpeg2000Loss)
{
    testFICompression("test-loss.jp2", Compression::Jpeg2000);
}

TEST(FIWrapper, openJpeg2000Lossless)
{
    testFICompression("test-lossless.jp2", Compression::Jpeg2000);
}

TEST(FIWrapper, openJpegFile)
{
    testFICompression("test.jpg", Compression::Jpeg);
}

TEST(FIWrapper, openPngFile)
{
	testFICompression("test.png", Compression::Png);
}

TEST(FIWrapper, lifetimeManagement)
{
    std::string filePath = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    for (int i = 0; i < 10; ++i) {
        FIWrapper wrapper(filePath);
		EXPECT_TRUE(wrapper.isValid());
        EXPECT_EQ(1, wrapper.getNumPages());
    }
}