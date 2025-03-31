#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include <string>
#include <tinyxml2.h>
#include <opencv2/imgproc.hpp>
#include <slideio/core/imagedrivermanager.hpp>

#include "slideio/core/tools/tools.hpp"
#include "slideio/drivers/ome-tiff/otimagedriver.hpp"
#include "slideio/drivers/ome-tiff/otscene.hpp"


namespace slideio
{
    class Slide;
}

using namespace slideio;
using namespace slideio::ometiff;
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
	Compression compression;
};

class OTImageDriverTests : public ::testing::Test {
protected:
	static void SetUpTestSuite() {
		ImageDriverManager::setLogLevel("WARNING");
		std::cerr << "SetUpTestSuite: Running before all tests\n";
	}
	static void TearDownTestSuite() {
	}
};


TEST_F(OTImageDriverTests, canOpenFile) {
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
TEST_F(OTImageDriverTests, openMultifileSlide) {
    std::string filePath = TestTools::getFullTestImagePath("ometiff", "Multifile/multifile-Z1.ome.tiff");
    slideio::ometiff::OTImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
	const int numScenes = slide->getNumScenes();
	ASSERT_EQ(numScenes, 1);
	const SceneInfo sceneInfo =
	{ "multifile", {0,0,18,24}, 1, 5,1,0,{1.e-6,1.e-6}, DataType::DT_Byte, Compression::Uncompressed };
	std::shared_ptr<CVScene> scene = slide->getSceneByName(sceneInfo.name);
	ASSERT_TRUE(scene != nullptr);
	EXPECT_EQ(scene->getRect(), sceneInfo.rect);
	EXPECT_EQ(scene->getNumChannels(), sceneInfo.numChannels);
	EXPECT_EQ(scene->getNumZSlices(), sceneInfo.numZSlices);
	EXPECT_EQ(scene->getNumTFrames(), sceneInfo.numTFrames);
	EXPECT_EQ(scene->getMagnification(), sceneInfo.magnification);
	EXPECT_DOUBLE_EQ(scene->getResolution().x, sceneInfo.res.x);
	EXPECT_DOUBLE_EQ(scene->getResolution().y, sceneInfo.res.y);
	EXPECT_EQ(scene->getChannelDataType(0), sceneInfo.dt);
	EXPECT_EQ(scene->getCompression(), sceneInfo.compression);
}

TEST_F(OTImageDriverTests, openMultiResolutionSlide) {
	const SceneInfo scenesInfo[] = {
		{"macro", {0,0,1616,4668}, 3, 1,1,0.60833,{1.6438445776255536e-5,1.6438445776255536e-5}, DataType::DT_Byte, Compression::Jpeg},
		{"Image:1", {0,0,39168,26048}, 3, 1,1,40.,{2.5e-7,2.5e-7}, DataType::DT_Byte, Compression::Jpeg},
		{"Image:2", {0,0,39360,23360}, 3, 1,1,40.,{2.5e-7,2.5e-7}, DataType::DT_Byte, Compression::Jpeg},
		{"Image:3", {0,0,39360,23360}, 3, 1,1,40.,{2.5e-7,2.5e-7}, DataType::DT_Byte, Compression::Jpeg},
		{"Image:4", {0,0,39168,26048}, 3, 1,1,40.,{2.5e-7,2.5e-7}, DataType::DT_Byte, Compression::Jpeg},
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
		EXPECT_EQ(scene->getRect(), sceneInfo.rect);
		EXPECT_EQ(scene->getNumChannels(), sceneInfo.numChannels);
		EXPECT_EQ(scene->getNumZSlices(), sceneInfo.numZSlices);
		EXPECT_EQ(scene->getNumTFrames(), sceneInfo.numTFrames);
		EXPECT_EQ(scene->getMagnification(), sceneInfo.magnification);
		EXPECT_EQ(scene->getResolution(), sceneInfo.res);
		EXPECT_EQ(scene->getChannelDataType(0), sceneInfo.dt);
		EXPECT_DOUBLE_EQ(scene->getResolution().x, sceneInfo.res.x);
		EXPECT_DOUBLE_EQ(scene->getResolution().y, sceneInfo.res.y);
		EXPECT_EQ(scene->getCompression(), sceneInfo.compression);
	}
}
