#include <gtest/gtest.h>

#include "slideio/converter/convertersvstools.hpp"
#include "tests/testlib/testtools.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"
#include "slideio/base/exceptions.hpp"


TEST(ConverterSVSTools, checkSVSRequirements)
{
	struct TestImages
	{
		std::string path;
		std::string driver;
		bool succeess;
	};
	TestImages tests[] = {
		{TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg"),
			"GDAL",
			true
		},
		{
			TestTools::getTestImagePath("gdal", "img_2448x2448_1x8bit_SRC_GRAY_ducks.png"),
			"GDAL",
			true
    	},
		{
			TestTools::getTestImagePath("dcm", "barre.dev/MR-MONO2-12-angio-an1"),
			"DCM",
			false
		}
};
	for(auto test : tests) {
		CVSlidePtr slide = slideio::ImageDriverManager::openSlide(test.path, test.driver);
		CVScenePtr scene = slide->getScene(0);
		if(test.succeess) {
			EXPECT_NO_THROW(slideio::ConverterSVSTools::checkSVSRequirements(scene));
		}
		else {
			EXPECT_THROW(slideio::ConverterSVSTools::checkSVSRequirements(scene), slideio::RuntimeError);
		}
	}
}
