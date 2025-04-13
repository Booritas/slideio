#include <filesystem>
#include <tinyxml2.h>
#include <gtest/gtest.h>
#include <opencv2/imgproc.hpp>

#include "slideio/drivers/ome-tiff/otdimensions.hpp"
#include "slideio/drivers/ome-tiff/tiffdata.hpp"
#include "slideio/imagetools/tifffiles.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "tests/testlib/testtools.hpp"

using namespace slideio;
using namespace slideio::ometiff;

class TiffDataTests : public ::testing::Test {
protected:
    void SetUp() override {
    }
};

TEST_F(TiffDataTests, init) {
    std::string filePath = TestTools::getFullTestImagePath("ometiff", "Subresolutions/retina_large.ome.tiff");
	std::string directoryPath = std::filesystem::path(filePath).parent_path().string();
    TIFFFiles files;
    libtiff::TIFF* tiff = files.getOrOpen(filePath);
	ASSERT_TRUE(tiff != nullptr);
	EXPECT_EQ(1, files.getNumberOfOpenFiles());
	std::vector<TiffDirectory> directories;
    TiffTools::scanFile(tiff, directories);
	ASSERT_GT(directories.size(), 0);
	const TiffDirectory& mainDir = directories[0];
    std::string description = mainDir.description;
	tinyxml2::XMLDocument doc;
	doc.Parse(description.c_str());
	tinyxml2::XMLElement* xmlImage = doc.RootElement()->FirstChildElement("Image");
	ASSERT_TRUE(xmlImage != nullptr);
	tinyxml2::XMLElement* xmlPixels = xmlImage->FirstChildElement("Pixels");
	ASSERT_TRUE(xmlPixels != nullptr);
	tinyxml2::XMLElement* xmlTiffData = xmlPixels->FirstChildElement("TiffData");
	ASSERT_TRUE(xmlTiffData != nullptr);
	TiffData tiffData;
	OTDimensions dimensions;
	dimensions.init("XYZCT", 2, 64, 1, 1);
	tiffData.init(directoryPath, &files, &dimensions, xmlTiffData);
	EXPECT_EQ(1, files.getNumberOfOpenFiles());
	EXPECT_EQ(tiffData.getFirstIFD(), 0);
	EXPECT_EQ(tiffData.getPlaneCount(), 1);
	EXPECT_EQ(tiffData.getChannelRange(), cv::Range( 0,1));
	EXPECT_EQ(tiffData.getZSliceRange(), cv::Range(0, 1));
	EXPECT_EQ(tiffData.getTFrameRange(), cv::Range(0, 1));
	EXPECT_EQ(tiffData.getFilePath(), filePath);
	EXPECT_EQ(tiffData.getTiffDirectoryCount(), 1);
	auto dir = tiffData.getTiffDirectory(0);
	EXPECT_EQ(dir.dirIndex, 0);
}

TEST_F(TiffDataTests, readTile) {
	std::string filePath = TestTools::getFullTestImagePath("ometiff", "Subresolutions/retina_large.ome.tiff");
	std::string directoryPath = std::filesystem::path(filePath).parent_path().string();
	TIFFFiles files;
	libtiff::TIFF* tiff = files.getOrOpen(filePath);
	ASSERT_TRUE(tiff != nullptr);
	EXPECT_EQ(1, files.getNumberOfOpenFiles());
	std::vector<TiffDirectory> directories;
	TiffTools::scanFile(tiff, directories);
	ASSERT_GT(directories.size(), 0);
	const TiffDirectory& mainDir = directories[0];
	std::string description = mainDir.description;
	tinyxml2::XMLDocument doc;
	doc.Parse(description.c_str());
	tinyxml2::XMLElement* xmlImage = doc.RootElement()->FirstChildElement("Image");
	ASSERT_TRUE(xmlImage != nullptr);
	tinyxml2::XMLElement* xmlPixels = xmlImage->FirstChildElement("Pixels");
	ASSERT_TRUE(xmlPixels != nullptr);
	tinyxml2::XMLElement* xmlTiffData = nullptr;
	for (tinyxml2::XMLElement* xmlCurrentTiffData = xmlPixels->FirstChildElement("TiffData");
		xmlCurrentTiffData != nullptr;
		xmlCurrentTiffData = xmlCurrentTiffData->NextSiblingElement("TiffData")) {
		int zSlice = xmlCurrentTiffData->IntAttribute("FirstZ");
		if (zSlice == 32) {
			xmlTiffData = xmlCurrentTiffData;
			break;
		}
	}
	ASSERT_TRUE(xmlTiffData != nullptr);
	TiffData tiffData;
	OTDimensions dimensions;
	dimensions.init("XYZCT", 2, 64, 1, 1);
	tiffData.init(directoryPath, &files, &dimensions, xmlTiffData);
	std::vector<int> channelIndices = { 0 };
	std::vector<cv::Mat> rasters(1);
	tiffData.readTile(channelIndices, 32, 0, 1, 0, rasters);
	EXPECT_FALSE(rasters[0].empty());
	TestTools::showRaster(rasters[0]);
}

TEST_F(TiffDataTests, readTileChannelsStripePlanar) {
	std::string filePath = TestTools::getFullTestImagePath("ometiff", "Subresolutions/retina_large.ome.tiff");
	std::string testFilePath = TestTools::getFullTestImagePath("ometiff", "Tests/retina_large.ome-page32-channel-0.tif");
	std::string directoryPath = std::filesystem::path(filePath).parent_path().string();
	TIFFFiles files;
	libtiff::TIFF* tiff = files.getOrOpen(filePath);
	ASSERT_TRUE(tiff != nullptr);
	EXPECT_EQ(1, files.getNumberOfOpenFiles());
	std::vector<TiffDirectory> directories;
	TiffTools::scanFile(tiff, directories);
	ASSERT_GT(directories.size(), 0);
	const TiffDirectory& mainDir = directories[0];
	std::string description = mainDir.description;
	tinyxml2::XMLDocument doc;
	doc.Parse(description.c_str());
	tinyxml2::XMLElement* xmlImage = doc.RootElement()->FirstChildElement("Image");
	ASSERT_TRUE(xmlImage != nullptr);
	tinyxml2::XMLElement* xmlPixels = xmlImage->FirstChildElement("Pixels");
	ASSERT_TRUE(xmlPixels != nullptr);
	tinyxml2::XMLElement* xmlTiffData = nullptr;
	int ifd = -1;
	for (tinyxml2::XMLElement* xmlCurrentTiffData = xmlPixels->FirstChildElement("TiffData");
		xmlCurrentTiffData != nullptr;
		xmlCurrentTiffData = xmlCurrentTiffData->NextSiblingElement("TiffData")) {
		int zSlice = xmlCurrentTiffData->IntAttribute("FirstZ");
		if (zSlice == 32) {
			xmlTiffData = xmlCurrentTiffData;
			ifd = xmlCurrentTiffData->IntAttribute("IFD");
			break;
		}
	}
	ASSERT_TRUE(xmlTiffData != nullptr);
	TiffData tiffData;
	OTDimensions dimensions;
	dimensions.init("XYZCT", 2, 64, 1, 1);
	tiffData.init(directoryPath, &files, &dimensions, xmlTiffData);
	const TiffDirectory& dir = tiffData.getTiffDirectory(0);
    const TiffDirectory& dir1 = dir.subdirectories[1];
	ASSERT_EQ(dir.dirIndex, ifd);
	std::vector<int> channelIndices = { 0 };
	cv::Mat raster;
	tiffData.readTileChannels(dir, 0, channelIndices,  raster);
	EXPECT_FALSE(raster.empty());
	cv::Mat testRaster;
	ImageTools::readGDALImage(testFilePath, testRaster);
	EXPECT_TRUE(TestTools::compareRastersEx(raster, testRaster));
	cv::resize(testRaster, testRaster, cv::Size(dir1.width, dir1.height));
	tiffData.readTileChannels(dir1, 0, channelIndices, raster);
	double sim = ImageTools::computeSimilarity2(raster, testRaster);
	EXPECT_TRUE(sim > 0.99);

}
