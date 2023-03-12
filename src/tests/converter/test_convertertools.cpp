#include <gtest/gtest.h>
#include <opencv2/highgui.hpp>

#include "tests/testlib/testtools.hpp"
#include "slideio/converter/convertertools.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"
#include "slideio/base/exceptions.hpp"


TEST(ConverterTools, computeNumZoomLevels)
{
    EXPECT_EQ(slideio::ConverterTools::computeNumZoomLevels(100, 100),1);
    EXPECT_EQ(slideio::ConverterTools::computeNumZoomLevels(100000, 100), 1);
    EXPECT_EQ(slideio::ConverterTools::computeNumZoomLevels(1000, 100), 1);
    EXPECT_EQ(slideio::ConverterTools::computeNumZoomLevels(1000, 1000), 1);
    EXPECT_EQ(slideio::ConverterTools::computeNumZoomLevels(1001, 1001), 2);
    EXPECT_EQ(slideio::ConverterTools::computeNumZoomLevels(16000, 16000), 5);
}

TEST(ConverterTools, readTile_normal)
{
	std::string path = TestTools::getTestImagePath("gdal",
		"Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
	CVSlidePtr slide = slideio::ImageDriverManager::openSlide(path, "GDAL");
	CVScenePtr scene = slide->getScene(0);
	{
        const cv::Rect tileRect(0, 0, 256, 180);
        const cv::Size tileSize(tileRect.size());
		cv::Mat tileRaster;
		slideio::ConverterTools::readTile(scene, 0, tileRect, tileRaster);
		cv::Mat blockRaster;
        const cv::Rect blockRect(tileRect);
		scene->readBlock(blockRect, blockRaster);
		EXPECT_EQ(cv::norm(blockRaster, tileRaster, cv::NORM_L2), 0);
	}
	{
		const cv::Rect tileRect(500, 600, 256, 180);
		const cv::Size tileSize(tileRect.size());
		cv::Mat tileRaster;
		slideio::ConverterTools::readTile(scene, 0, tileRect, tileRaster);
		cv::Mat blockRaster;
		const cv::Rect blockRect(tileRect);
		scene->readBlock(blockRect, blockRaster);
		EXPECT_EQ(cv::norm(blockRaster, tileRaster, cv::NORM_L2), 0);
	}
}

TEST(ConverterTools, readTile_scaled)
{
	std::string path = TestTools::getTestImagePath("gdal",
		"Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
	CVSlidePtr slide = slideio::ImageDriverManager::openSlide(path, "GDAL");
	CVScenePtr scene = slide->getScene(0);
	{
		const int zoomLevel = 1;
		cv::Rect sceneRect = scene->getRect();
		const cv::Rect blockRect(55, 75, 512, 1024);
		cv::Mat tileRaster;
		slideio::ConverterTools::readTile(scene, zoomLevel, blockRect, tileRaster);
		cv::Size tileSize = slideio::ConverterTools::scaleSize(blockRect.size(), zoomLevel, true);
		cv::Mat blockRaster;
		scene->readResampledBlock(blockRect, tileSize, blockRaster);
		EXPECT_EQ(cv::norm(blockRaster, tileRaster, cv::NORM_L2), 0);
	}
	{
		const int zoomLevel = 2;
		cv::Rect sceneRect = scene->getRect();
		const cv::Rect blockRect(55, 75, 512, 1024);
		cv::Mat tileRaster;
		slideio::ConverterTools::readTile(scene, zoomLevel, blockRect, tileRaster);
		cv::Size tileSize = slideio::ConverterTools::scaleSize(blockRect.size(), zoomLevel, true);
		cv::Mat blockRaster;
		scene->readResampledBlock(blockRect, tileSize, blockRaster);
		EXPECT_EQ(cv::norm(blockRaster, tileRaster, cv::NORM_L2), 0);
	}
}

TEST(ConverterTools, readTile_edge)
{
	std::string path = TestTools::getTestImagePath("gdal",
		"Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
	CVSlidePtr slide = slideio::ImageDriverManager::openSlide(path, "GDAL");
	CVScenePtr scene = slide->getScene(0);
	cv::Rect sceneRect = scene->getRect();
	{
		const int zoomLevel = 1;
		const cv::Rect blockRect(5200, 5600, 1024, 1024);
		cv::Mat tileRaster;
		slideio::ConverterTools::readTile(scene, zoomLevel, blockRect, tileRaster);
		cv::Rect validSceneRect = sceneRect & blockRect;
	    cv::Size tileSize = slideio::ConverterTools::scaleSize(validSceneRect.size(), zoomLevel, true);
		cv::Mat blockRaster;
		scene->readResampledBlock(validSceneRect, tileSize, blockRaster);
		cv::Rect validZoneRect(0, 0, tileSize.width, tileSize.height);
		EXPECT_EQ(cv::norm(blockRaster, tileRaster(validZoneRect), cv::NORM_L2), 0);
	}
}

TEST(ConverterTools, scaleSize_downScale)
{
	{
		// zero zoom level
		cv::Size size(640, 480);
		cv::Size scaledSize = slideio::ConverterTools::scaleSize(size, 0);
		EXPECT_EQ(scaledSize.width, 640);
		EXPECT_EQ(scaledSize.height, 480);
	}
	{
		// positive zoom level
		cv::Size size(640, 480);
		cv::Size scaledSize = slideio::ConverterTools::scaleSize(size, 2);
		EXPECT_EQ(scaledSize.width, 160);
		EXPECT_EQ(scaledSize.height, 120);
	}
	{
		// zero size
		cv::Size size(0, 0);
		cv::Size scaledSize = slideio::ConverterTools::scaleSize(size, 2);
		EXPECT_EQ(scaledSize.width, 0);
		EXPECT_EQ(scaledSize.height, 0);
	}
	{
		// negative zoom level
		cv::Size size(0, 0);
		EXPECT_THROW(slideio::ConverterTools::scaleSize(size, -2), slideio::RuntimeError);
	}
}

TEST(ConverterTools, scaleSize_upScale)
{
	{
		// positive zoom level
		cv::Size size(160, 120);
		cv::Size scaledSize = slideio::ConverterTools::scaleSize(size, 2, false);
		EXPECT_EQ(scaledSize.width, 640);
		EXPECT_EQ(scaledSize.height, 480);
	}
	{
		// zero size
		cv::Size size(0, 0);
		cv::Size scaledSize = slideio::ConverterTools::scaleSize(size, 2, false);
		EXPECT_EQ(scaledSize.width, 0);
		EXPECT_EQ(scaledSize.height, 0);
	}
	{
		// negative zoom level
		cv::Size size(0, 0);
		EXPECT_THROW(slideio::ConverterTools::scaleSize(size, -2, false), slideio::RuntimeError);
	}
}

TEST(ConverterTools, scaleRect)
{
	{
		cv::Rect rect(10, 20, 30, 40);
		int zoomLevel = 2;
		bool downScale = false;
		cv::Rect scaledRect = slideio::ConverterTools::scaleRect(rect, zoomLevel, downScale);
		EXPECT_EQ(scaledRect.x, 40);
		EXPECT_EQ(scaledRect.y, 80);
		EXPECT_EQ(scaledRect.width, 120);
		EXPECT_EQ(scaledRect.height, 160);
	}
	{
		cv::Rect rect(10, 20, 30, 40);
		int zoomLevel = 0;
		bool downScale = false;
		cv::Rect scaledRect = slideio::ConverterTools::scaleRect(rect, zoomLevel, downScale);
		EXPECT_EQ(scaledRect.x, 10);
		EXPECT_EQ(scaledRect.y, 20);
		EXPECT_EQ(scaledRect.width, 30);
		EXPECT_EQ(scaledRect.height, 40);
	}
	{
		cv::Rect rect(10, 20, 30, 40);
		int zoomLevel = -2;
		bool downScale = false;
		ASSERT_THROW(slideio::ConverterTools::scaleRect(rect, zoomLevel, downScale), slideio::RuntimeError);
	}
	{
		cv::Rect rect(10, 20, 30, 40);
		int zoomLevel = 0;
		bool downScale = false;
		cv::Rect scaledRect = slideio::ConverterTools::scaleRect(rect, zoomLevel, downScale);
		EXPECT_EQ(scaledRect.x, 10);
		EXPECT_EQ(scaledRect.y, 20);
		EXPECT_EQ(scaledRect.width, 30);
		EXPECT_EQ(scaledRect.height, 40);
	}
	{
		cv::Rect rect(10, 20, 30, 40);
		int zoomLevel = 2;
		bool downScale = true;
		cv::Rect scaledRect = slideio::ConverterTools::scaleRect(rect, zoomLevel, downScale);
		EXPECT_EQ(scaledRect.x, 2);
		EXPECT_EQ(scaledRect.y, 5);
		EXPECT_EQ(scaledRect.width, 7);
		EXPECT_EQ(scaledRect.height, 10);
	}
}

TEST(ConverterTools, computeZoomLevelRect)
{
	{
		cv::Rect rect(0, 0, 100, 200);
		cv::Size tile(256, 256);
		cv::Rect levelRect = slideio::ConverterTools::computeZoomLevelRect(rect, tile, 0);
		cv::Rect expectedRect(0, 0, 256, 256);
		EXPECT_EQ(levelRect, expectedRect);
	}
	{
		cv::Rect rect(0, 0, 256, 256);
		cv::Size tile(256, 256);
		cv::Rect levelRect = slideio::ConverterTools::computeZoomLevelRect(rect, tile, 0);
		cv::Rect expectedRect(0, 0, 256, 256);
		EXPECT_EQ(levelRect, expectedRect);
	}
	{
		cv::Rect rect(0, 0, 450, 700);
		cv::Size tile(100, 200);
		cv::Rect levelRect = slideio::ConverterTools::computeZoomLevelRect(rect, tile, 0);
		cv::Rect expectedRect(0, 0, 500, 800);
		EXPECT_EQ(levelRect, expectedRect);
	}
	{
		cv::Rect rect(0, 0, 900, 1400);
		cv::Size tile(100, 200);
		cv::Rect levelRect = slideio::ConverterTools::computeZoomLevelRect(rect, tile, 1);
		cv::Rect expectedRect(0, 0, 500, 800);
		EXPECT_EQ(levelRect, expectedRect);
	}
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

	slideio::ConverterTools::convertTo32bitChannels(inputData, width, height, numChannels, channels);

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
