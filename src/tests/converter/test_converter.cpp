#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include "slideio/converter/converter.hpp"
#include "slideio/slideio/slideio.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/base/exceptions.hpp"
#include <boost/filesystem.hpp>

#include "slideio/core/cvslide.hpp"
#include "slideio/imagetools/tempfile.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"


TEST(Converter, convertGDALJpeg)
{
	std::string path = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
	SlidePtr slide = slideio::openSlide(path, "GDAL");
	ScenePtr scene = slide->getScene(0);
	auto sceneRect = scene->getRect();
	int sceneWidth = std::get<2>(sceneRect);
	int sceneHeight = std::get<3>(sceneRect);
	ASSERT_TRUE(scene.get() != nullptr);

	slideio::TempFile tmp("svs");
	std::string outputPath = tmp.getPath().string();
	if(boost::filesystem::exists(outputPath)) {
		boost::filesystem::remove(outputPath);
	}
	slideio::ImageTools::JpegEncodeParameters encodeParameters(99);
    slideio::ConverterParameters parameters(&encodeParameters);
	parameters.driver = "SVS";
	slideio::convertScene(scene, parameters, outputPath);
	SlidePtr svsSlide = slideio::openSlide(outputPath, "SVS");
	ScenePtr svsScene = svsSlide->getScene(0);
	auto svsRect = svsScene->getRect();
	EXPECT_EQ(sceneWidth, std::get<2>(svsRect));
	EXPECT_EQ(sceneHeight, std::get<3>(svsRect));
	int dataSize = sceneHeight * sceneWidth * scene->getNumChannels();
	std::vector<uint8_t> svsBuffer(dataSize);
	svsScene->readBlock(sceneRect, svsBuffer.data(), svsBuffer.size());
	std::vector<uint8_t> gdalBuffer(dataSize);
	scene->readBlock(sceneRect, gdalBuffer.data(), gdalBuffer.size());
	cv::Mat svsImage(sceneHeight, sceneWidth, CV_8UC3, svsBuffer.data());
	cv::Mat gdalImage(sceneHeight, sceneWidth, CV_8UC3, gdalBuffer.data());
    double sim = slideio::ImageTools::computeSimilarity(svsImage, gdalImage);
	EXPECT_LE(0.99, sim);
}

TEST(Converter, convertGDALJp2K)
{
	std::string path = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
	SlidePtr slide = slideio::openSlide(path, "GDAL");
	ScenePtr scene = slide->getScene(0);
	auto sceneRect = scene->getRect();
	int sceneWidth = std::get<2>(sceneRect);
	int sceneHeight = std::get<3>(sceneRect);
	ASSERT_TRUE(scene.get() != nullptr);

	// slideio::TempFile tmp("svs");
	// std::string outputPath = tmp.getPath().string();
	std::string outputPath = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.svs");
	if (boost::filesystem::exists(outputPath)) {
		boost::filesystem::remove(outputPath);
	}
	slideio::ImageTools::JP2KEncodeParameters encodeParameters;
	slideio::ConverterParameters parameters(&encodeParameters);
	parameters.driver = "SVS";
	slideio::convertScene(scene, parameters, outputPath);
	SlidePtr svsSlide = slideio::openSlide(outputPath, "SVS");
	ScenePtr svsScene = svsSlide->getScene(0);
	auto svsRect = svsScene->getRect();
	EXPECT_EQ(sceneWidth, std::get<2>(svsRect));
	EXPECT_EQ(sceneHeight, std::get<3>(svsRect));
	int dataSize = sceneHeight * sceneWidth * scene->getNumChannels();
	std::vector<uint8_t> svsBuffer(dataSize);
	svsScene->readBlock(sceneRect, svsBuffer.data(), svsBuffer.size());
	std::vector<uint8_t> gdalBuffer(dataSize);
	scene->readBlock(sceneRect, gdalBuffer.data(), gdalBuffer.size());
	cv::Mat svsImage(sceneHeight, sceneWidth, CV_8UC3, svsBuffer.data());
	cv::Mat gdalImage(sceneHeight, sceneWidth, CV_8UC3, gdalBuffer.data());
	//TestTools::showRaster(svsImage);
	double sim = slideio::ImageTools::computeSimilarity(svsImage, gdalImage);
	EXPECT_LE(0.99, sim);
}

TEST(Converter, nullScene)
{
	std::string outputPath = TestTools::getTestImagePath("gdal", "test.svs");
	slideio::ImageTools::JpegEncodeParameters encodeParameters(99);
	slideio::ConverterParameters parameters(&encodeParameters);
	parameters.driver = "SVS";
	ASSERT_THROW(slideio::convertScene(nullptr, parameters, outputPath), slideio::RuntimeError);
}

TEST(Converter, unspecifiedDriver)
{
	std::string path = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
	SlidePtr slide = slideio::openSlide(path, "GDAL");
	ScenePtr scene = slide->getScene(0);
	ASSERT_TRUE(scene.get() != nullptr);
	
	slideio::ImageTools::JpegEncodeParameters encodeParameters(99);
	slideio::ConverterParameters parameters(&encodeParameters);
	std::string outputPath = TestTools::getTestImagePath("gdal", "test.svs");
	ASSERT_THROW(slideio::convertScene(scene, parameters, outputPath), slideio::RuntimeError);
}

TEST(Converter, unsupportedDriver)
{
	std::string path = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
	SlidePtr slide = slideio::openSlide(path, "GDAL");
	ScenePtr scene = slide->getScene(0);
	ASSERT_TRUE(scene.get() != nullptr);
	
	slideio::ImageTools::JpegEncodeParameters encodeParameters(99);
	slideio::ConverterParameters parameters(&encodeParameters);
	parameters.driver = "GDAL";
	std::string outputPath = TestTools::getTestImagePath("gdal", "test.svs");
	ASSERT_THROW(slideio::convertScene(scene, parameters, outputPath), slideio::RuntimeError);
}

TEST(Converter, outputPathExists)
{
	std::string path = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
	SlidePtr slide = slideio::openSlide(path, "GDAL");
	ScenePtr scene = slide->getScene(0);
	ASSERT_TRUE(scene.get() != nullptr);
	
	slideio::ImageTools::JpegEncodeParameters encodeParameters(99);
	slideio::ConverterParameters parameters(&encodeParameters);
	parameters.driver = "SVS";
	ASSERT_THROW(slideio::convertScene(scene, parameters, path), slideio::RuntimeError);
}

