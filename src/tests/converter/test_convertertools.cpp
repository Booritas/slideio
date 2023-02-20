#include <gtest/gtest.h>
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
	cv::Rect tileRect(0, 0, 256, 180);
	cv::Size tileSize(tileRect.size());
	cv::Mat tileRaster;
	slideio::ConverterTools::readTile(scene, 0, tileRect, tileSize, tileRaster);
	cv::Mat blockRaster;
	cv::Rect blockRect(0, 0, 256, 180);
	scene->readBlock(blockRect, blockRaster);
	EXPECT_EQ(cv::norm(blockRaster, tileRaster, cv::NORM_L2), 0);
}

TEST(ConverterTools, readTile_scaled)
{
	std::string path = TestTools::getTestImagePath("gdal",
		"Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
	CVSlidePtr slide = slideio::ImageDriverManager::openSlide(path, "GDAL");
	CVScenePtr scene = slide->getScene(0);
	cv::Rect sceneRect = scene->getRect();
	cv::Rect tileRect(0, 0, 256,512);
	cv::Size tileSize(tileRect.size());
	cv::Mat tileRaster;
	slideio::ConverterTools::readTile(scene, 1, tileRect, tileSize, tileRaster);
	cv::Mat blockRaster;
	cv::Rect blockRect(0, 0, 256, 180);
	scene->readBlock(blockRect, blockRaster);
	EXPECT_EQ(cv::norm(blockRaster, tileRaster, cv::NORM_L2), 0);
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
