#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include <string>
#include <tinyxml2.h>
#include <opencv2/imgproc.hpp>

#include "slideio/core/tools/tools.hpp"
#include "slideio/drivers/ome-tiff/otimagedriver.hpp"
#include "slideio/drivers/ome-tiff/otscene.hpp"


namespace slideio
{
    class Slide;
}

using namespace slideio;
using namespace slideio::ometiff;

TEST(OTImageDriver, canOpenFile) {
    const std::string allowedSuffixes[] = { ".ome.tif",".ome.tiff", ".ome.tf2", ".ome.tf8", ".ome.btf" };
    const std::string disallowedSuffixes[] = { ".ometif",".ometiff", ".ometf2", ".ometf8", ".omebtf" };
    OTImageDriver driver;
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
TEST(OTImageDriver, openMultifileSlide) {
    std::string filePath = TestTools::getFullTestImagePath("ometiff", "Multifile/multifile-Z1.ome.tiff");
    slideio::ometiff::OTImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
}

TEST(OTImageDriver, openMultiResolutionSlide) {
	struct SceneInfo
	{
		std::string name;
		cv::Rect rect;
		int numChannels;
		int numZSlices;
		int numTFrames;
		double magnification;
		Resolution res;
		DataType dt;
	};
	const SceneInfo scenesInfo[] = {
		{"macro", {0,0,1616,4668}, 3, 1,1,0,{0.,0.}, DataType::DT_Byte},
		{"Image:1", {0,0,39168,26048}, 3, 1,1,0,{0.,0.}, DataType::DT_Byte},
		{"Image:2", {0,0,39360,23360}, 3, 1,1,0,{0.,0.}, DataType::DT_Byte},
		{"Image:3", {0,0,39360,23360}, 3, 1,1,0,{0.,0.}, DataType::DT_Byte},
		{"Image:4", {0,0,39168,26048}, 3, 1,1,0,{0.,0.}, DataType::DT_Byte},
	};
    std::string filePath = TestTools::getFullTestImagePath("ometiff", "Subresolutions/Leica-2.ome.tiff");
    slideio::ometiff::OTImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
	ASSERT_EQ(numScenes, 5);
	for (auto& sceneInfo : scenesInfo) {
		std::shared_ptr<CVScene> scene = slide->getSceneByName(sceneInfo.name);
		ASSERT_TRUE(scene != nullptr);
		EXPECT_EQ(scene->getNumChannels(), sceneInfo.numChannels);
		EXPECT_EQ(scene->getNumZSlices(), sceneInfo.numZSlices);
		EXPECT_EQ(scene->getNumTFrames(), sceneInfo.numTFrames);
		EXPECT_EQ(scene->getMagnification(), sceneInfo.magnification);
		EXPECT_EQ(scene->getResolution(), sceneInfo.res);
		EXPECT_EQ(scene->getChannelDataType(0), sceneInfo.dt);
	}
}
