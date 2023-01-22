#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include "slideio/converter/converter.hpp"
#include "slideio/slideio/slideio.hpp"
#include "slideio/slideio/scene.hpp"


TEST(Converter, convertGDAL)
{
	std::string path = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
	SlidePtr slide = slideio::openSlide(path, "GDAL");
	ScenePtr scene = slide->getScene(0);
	ASSERT_TRUE(scene.get() != nullptr);

	std::map<std::string, std::string> parameters;
	std::string outputPath = TestTools::getTestImagePath("gdal", "test.svs");
	slideio::convertScene(scene, parameters, outputPath);
}