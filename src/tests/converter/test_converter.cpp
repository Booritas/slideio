#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include "slideio/converter/converter.hpp"
#include "slideio/slideio/slideio.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/base/exceptions.hpp"
#include <boost/filesystem.hpp>

#include "slideio/core/cvslide.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"


TEST(Converter, convertGDAL)
{
	std::string path = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
	SlidePtr slide = slideio::openSlide(path, "GDAL");
	ScenePtr scene = slide->getScene(0);
	ASSERT_TRUE(scene.get() != nullptr);
	
	std::string outputPath = TestTools::getTestImagePath("svs", "tests/gdal-test.svs");
	if(boost::filesystem::exists(outputPath)) {
		boost::filesystem::remove(outputPath);
	}
	slideio::ImageTools::JpegEncodeParameters encodeParameters(99);
    slideio::ConverterParameters parameters(&encodeParameters);
	parameters.driver = "SVS";
	slideio::convertScene(scene, parameters, outputPath);
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

