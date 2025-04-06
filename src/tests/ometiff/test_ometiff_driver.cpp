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
struct ZoomLevelInfo
{
	int level;
	Size size;
	double scale;
	double magnification;
	Size tileSize;
};

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
	int levels = 0;
	int levelInfoIndex = -1;
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
	{ "multifile", {0,0,18,24}, 1, 5,1,0,{1.e-6,1.e-6}, DataType::DT_Byte, Compression::Uncompressed, 1 };
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
		{"macro", {0,0,1616,4668}, 3, 1,1,0.60833,{1.6438445776255536e-5,1.6438445776255536e-5}, DataType::DT_Byte, Compression::Jpeg, 3},
		{"Image:1", {0,0,39168,26048}, 3, 1,1,40.,{2.5e-7,2.5e-7}, DataType::DT_Byte, Compression::Jpeg, 6},
		{"Image:2", {0,0,39360,23360}, 3, 1,1,40.,{2.5e-7,2.5e-7}, DataType::DT_Byte, Compression::Jpeg, 6},
		{"Image:3", {0,0,39360,23360}, 3, 1,1,40.,{2.5e-7,2.5e-7}, DataType::DT_Byte, Compression::Jpeg, 6},
		{"Image:4", {0,0,39168,26048}, 3, 1,1,40.,{2.5e-7,2.5e-7}, DataType::DT_Byte, Compression::Jpeg, 6},
	};
	const ZoomLevelInfo macroZoomLevels[] = {
		0, {1616, 4668}, 1.0, 0.60833, {0, 0},
	    1, {404, 1167}, 1./4., 0.60833/4., {0, 0},
		2, {101, 291}, 1./16., 0.60833/16., {0, 0},
	};
	const ZoomLevelInfo image4ZoomLevels[] = {
		0, {39168, 26048}, 1.0, 40., {512, 512},
		1, {9792, 6512}, 1./4., 40. / 4., {512, 512},
		2, {2448, 1628}, 1./16., 40. / 16., {0, 0},
		3, {612, 407}, 1./64., 40. / 64., {0, 0},
		4, {153, 101}, 1./256., 40. / 256., {0, 0},
		5, {38, 25}, 0.00097017973856209153, 0.038807189542483661, {0, 0},
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
		EXPECT_EQ(scene->getNumZoomLevels(), sceneInfo.levels);
		if (sceneInfo.name == "macro") {
			for (auto& zoomLevel : macroZoomLevels) {
				EXPECT_EQ(scene->getZoomLevelInfo(zoomLevel.level)->getSize(), zoomLevel.size);
				EXPECT_DOUBLE_EQ(scene->getZoomLevelInfo(zoomLevel.level)->getScale(), zoomLevel.scale);
				EXPECT_DOUBLE_EQ(scene->getZoomLevelInfo(zoomLevel.level)->getMagnification(), zoomLevel.magnification);
				EXPECT_EQ(scene->getZoomLevelInfo(zoomLevel.level)->getTileSize(), zoomLevel.tileSize);
			}
		}
		else if (sceneInfo.name == "Image:4") {
			for (auto& zoomLevel : image4ZoomLevels) {
				EXPECT_EQ(scene->getZoomLevelInfo(zoomLevel.level)->getSize(), zoomLevel.size);
				EXPECT_DOUBLE_EQ(scene->getZoomLevelInfo(zoomLevel.level)->getScale(), zoomLevel.scale);
				EXPECT_DOUBLE_EQ(scene->getZoomLevelInfo(zoomLevel.level)->getMagnification(), zoomLevel.magnification);
				EXPECT_EQ(scene->getZoomLevelInfo(zoomLevel.level)->getTileSize(), zoomLevel.tileSize);
			}
		}
	}
}

TEST_F(OTImageDriverTests, openFluorescentSlide) {
	std::string filePath = TestTools::getFullTestImagePath("ometiff", "Subresolutions/retina_large.ome.tiff");
	slideio::ometiff::OTImageDriver driver;
	std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
	ASSERT_TRUE(slide != nullptr);
	const int numScenes = slide->getNumScenes();
	ASSERT_EQ(numScenes, 2);
	const SceneInfo sceneInfos [] = {
	{ "retina_large.ims Resolution Level 1", {0,0,2048,1567}, 2, 64,1,0,{2.2905761973056524e-08,2.2898531953649078e-08}, DataType::DT_Byte, Compression::Zlib, 3, 0 },
	{ "retina_large.ims Resolution Level 2", {0,0,256,195}, 2, 32,1,0,{1.8324609578445219e-07,1.8401025627165894e-07}, DataType::DT_Byte, Compression::Zlib, 1, -1 }
	};

	const std::vector<ZoomLevelInfo> zoomLevelsInfo0 = {
		{0, {2048, 1567}, 1.0, 0., {0, 0}},
		{1, {1024, 783}, 1. / 2., 0., {0, 0}},
		{2, {512, 391}, 1. / 4., 0., {0, 0}},
	};

	const std::vector<std::vector<ZoomLevelInfo>> zoomLevelInfos = {
		zoomLevelsInfo0,
	};


	for (auto& sceneInfo : sceneInfos) {
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
		EXPECT_EQ(scene->getNumZoomLevels(), sceneInfo.levels);
		if (sceneInfo.levelInfoIndex >= 0 ) {
            const std::vector<ZoomLevelInfo>& zoomLevelInfo = zoomLevelInfos[sceneInfo.levelInfoIndex];
			for (auto& zoomLevel : zoomLevelInfo) {
				EXPECT_EQ(scene->getZoomLevelInfo(zoomLevel.level)->getSize(), zoomLevel.size);
				EXPECT_DOUBLE_EQ(scene->getZoomLevelInfo(zoomLevel.level)->getScale(), zoomLevel.scale);
				EXPECT_DOUBLE_EQ(scene->getZoomLevelInfo(zoomLevel.level)->getMagnification(), zoomLevel.magnification);
				EXPECT_EQ(scene->getZoomLevelInfo(zoomLevel.level)->getTileSize(), zoomLevel.tileSize);
			}
		}
	}

	//auto scene = slide->getSceneByName("retina_large.ims Resolution Level 1");
	//cv::Rect roi = { 651, 724, 304, 196 };
	//cv::Mat blockRaster;
	//scene->readResampled4DBlockChannels(roi, roi.size(), { 0 }, { 32, 33 }, { 0, 1 }, blockRaster);
	//TestTools::showRaster(blockRaster);
}
