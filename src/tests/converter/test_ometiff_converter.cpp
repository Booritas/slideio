#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include "slideio/converter/converter.hpp"
#include "slideio/slideio/slideio.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/base/exceptions.hpp"
#include <filesystem>

#include "slideio/converter/converterparameters.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/core/tools/tempfile.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"
#include "slideio/core/tools/tools.hpp"

using namespace slideio;
using namespace slideio::converter;

TEST(Converter, convertGdalToOmetiff)
{
	std::string path = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
	SlidePtr slide = slideio::openSlide(path, "GDAL");
	ScenePtr scene = slide->getScene(0);
	auto sceneRect = scene->getRect();
	int sceneWidth = std::get<2>(sceneRect);
	int sceneHeight = std::get<3>(sceneRect);
	ASSERT_TRUE(scene.get() != nullptr);

	slideio::TempFile tmp("ome.tiff");
	std::string outputPath = tmp.getPath().string();
	if(std::filesystem::exists(outputPath)) {
		std::filesystem::remove(outputPath);
	}
	OMETIFFJpegConverterParameters parameters;
	parameters.setQuality(99);
	convertScene(scene, parameters, outputPath);
	SlidePtr omeSlide = openSlide(outputPath, "OMETIFF");
	ScenePtr omeScene = omeSlide->getScene(0);
	auto svsRect = omeScene->getRect();
	EXPECT_EQ(sceneWidth, std::get<2>(svsRect));
	EXPECT_EQ(sceneHeight, std::get<3>(svsRect));
	int dataSize = sceneHeight * sceneWidth * scene->getNumChannels();
	std::vector<uint8_t> svsBuffer(dataSize);
	omeScene->readBlock(sceneRect, svsBuffer.data(), svsBuffer.size());
	std::vector<uint8_t> gdalBuffer(dataSize);
	scene->readBlock(sceneRect, gdalBuffer.data(), gdalBuffer.size());
	cv::Mat ometiffImage(sceneHeight, sceneWidth, CV_8UC3, svsBuffer.data());
	cv::Mat gdalImage(sceneHeight, sceneWidth, CV_8UC3, gdalBuffer.data());
    double sim = slideio::ImageTools::computeSimilarity(ometiffImage, gdalImage);
	EXPECT_LE(0.99, sim);
}

TEST(Converter, convertGdalToOmetiffJp2K)
{
	std::string path = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
	SlidePtr slide = slideio::openSlide(path, "GDAL");
	ScenePtr scene = slide->getScene(0);
	auto sceneRect = scene->getRect();
	int sceneWidth = std::get<2>(sceneRect);
	int sceneHeight = std::get<3>(sceneRect);
	ASSERT_TRUE(scene.get() != nullptr);

	slideio::TempFile tmp("ome.tiff");
	std::string outputPath = tmp.getPath().string();
	if (std::filesystem::exists(outputPath)) {
		std::filesystem::remove(outputPath);
	}
	OMETIFFJp2KConverterParameters parameters;
	convertScene(scene, parameters, outputPath);
	SlidePtr omeSlide = openSlide(outputPath, "OMETIFF");
	ScenePtr omeScene = omeSlide->getScene(0);
	auto omeRect = omeScene->getRect();
	EXPECT_EQ(sceneWidth, std::get<2>(omeRect));
	EXPECT_EQ(sceneHeight, std::get<3>(omeRect));
	int dataSize = sceneHeight * sceneWidth * scene->getNumChannels();
	std::vector<uint8_t> svsBuffer(dataSize);
	EXPECT_EQ(Compression::Jpeg2000, omeScene->getCompression());
	omeScene->readBlock(sceneRect, svsBuffer.data(), svsBuffer.size());
	std::vector<uint8_t> gdalBuffer(dataSize);
	scene->readBlock(sceneRect, gdalBuffer.data(), gdalBuffer.size());
	cv::Mat ometiffImage(sceneHeight, sceneWidth, CV_8UC3, svsBuffer.data());
	cv::Mat gdalImage(sceneHeight, sceneWidth, CV_8UC3, gdalBuffer.data());
	double sim = slideio::ImageTools::computeSimilarity(ometiffImage, gdalImage);
	EXPECT_LE(0.99, sim);
}
