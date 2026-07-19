#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include <string>
#include <tinyxml2.h>
#include <opencv2/imgproc.hpp>
#include <slideio/slideio/imagedrivermanager.hpp>
#include "slideio/core/tools/tools.hpp"
#include "slideio/drivers/svs/svsimagedriver.hpp"
#include "slideio/imagetools/smallimage.hpp"
#include "slideio/slideio/slideio.hpp"
#include "slideio/drivers/svs/svsimagedriver.hpp"


namespace slideio
{
    class Slide;
}

using namespace slideio;

// struct ZoomLevelInfo
// {
// 	int level;
// 	Size size;
// 	double scale;
// 	double magnification;
// 	Size tileSize;
// };

// struct SceneInfo
// {
// 	std::string name;
// 	cv::Rect rect;
// 	int numChannels;
// 	int numZSlices;
// 	int numTFrames;
// 	double magnification;
// 	Resolution res;
// 	DataType dt;
// 	Compression compression;
// 	int levels = 0;
// 	int levelInfoIndex = -1;
// 	double zResolution = 0.0;
// 	double tResolution = 0.0;
// };

class PhtiffImageDriverTests : public ::testing::Test {
protected:
	static void SetUpTestSuite() {
		ImageDriverManager::setLogLevel("WARNING");
		std::cerr << "SetUpTestSuite: Running before all tests\n";
	}
	static void TearDownTestSuite() {
	}
};


TEST_F(PhtiffImageDriverTests, canOpenFile) {
    const std::string allowedSuffixes[] = { ".tif",".tiff" };
    const std::string disallowedSuffixes[] = { ".ometif",".ometiff", ".ometf2", ".ometf8", ".omebtf" };
    SVSImageDriver driver("PHTIFF");
	for(std::string suffix : allowedSuffixes) {
		std::string filePath = "/projects/ometiff" + suffix;
		EXPECT_TRUE(driver.canOpenFile(filePath));
	}
	for (std::string suffix : allowedSuffixes) {
        std::transform(suffix.begin(), suffix.end(), suffix.begin(),
            [](unsigned char c) { return std::toupper(c); });
		std::string filePath = "/projects/ometiff" + suffix;
		EXPECT_TRUE(driver.canOpenFile(filePath));
	}
	for (std::string suffix : disallowedSuffixes) {
		std::string filePath = "/projects/ometiff" + suffix;
		EXPECT_FALSE(driver.canOpenFile(filePath));
	}
}

TEST_F(PhtiffImageDriverTests, openSlide) {
	std::string filePath = TestTools::getFullTestImagePath("philips", "Philips-3.tiff");
	auto slide = slideio::openSlide(filePath, "PHTIFF");
	ASSERT_TRUE(slide != nullptr);
}

