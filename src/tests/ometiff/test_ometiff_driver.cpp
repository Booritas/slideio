#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include <string>
#include <tinyxml2.h>
#include <opencv2/imgproc.hpp>
#include <slideio/slideio/imagedrivermanager.hpp>
#include "slideio/core/tools/tools.hpp"
#include "slideio/drivers/ome-tiff/otimagedriver.hpp"
#include "slideio/drivers/ome-tiff/otscene.hpp"
#include "slideio/slideio/slideio.hpp"


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
	double zResolution = 0.0;
	double tResolution = 0.0;
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

TEST_F(OTImageDriverTests, openSlide) {
	std::string filePath = TestTools::getFullTestImagePath("ometiff", "Multifile/multifile-Z1.ome.tiff");
	auto slide = slideio::openSlide(filePath, "OMETIFF");
	ASSERT_TRUE(slide != nullptr);
	slide = slideio::openSlide(filePath, "AUTO");
	ASSERT_TRUE(slide != nullptr);
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

TEST_F(OTImageDriverTests, openMultifileExternalMetadata) {
	std::string filePath = TestTools::getFullTestImagePath("ometiff", "Multifile2/multifile-Z1.ome.tiff");
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
	{ "retina_large.ims Resolution Level 1", {0,0,2048,1567}, 2, 64,1,0,{2.2905761973056524e-08,2.2898531953649078e-08}, DataType::DT_Byte, Compression::Zlib, 3, 0, 0.2e-6 },
	{ "retina_large.ims Resolution Level 2", {0,0,256,195}, 2, 32,1,0,{1.8324609578445219e-07,1.8401025627165894e-07}, DataType::DT_Byte, Compression::Zlib, 1, -1, 0.4e-6 }
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
		EXPECT_DOUBLE_EQ(scene->getZSliceResolution(), sceneInfo.zResolution);
		EXPECT_DOUBLE_EQ(scene->getTFrameResolution(), sceneInfo.tResolution);
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
	std::shared_ptr<CVScene> scene = slide->getSceneByName("retina_large.ims Resolution Level 1");
	std::shared_ptr<OTScene> otScene = std::static_pointer_cast<OTScene>(scene);
	int files = otScene->getNumTiffFiles();
	EXPECT_EQ(files, 1);
	EXPECT_EQ(otScene->getNumTiffDataItems(), 128);
}

TEST_F(OTImageDriverTests, TIFFFiles) {
	TIFFFiles files;
	std::string filePath1 = TestTools::getFullTestImagePath("ometiff", "Subresolutions/retina_large.ome.tiff");
	std::string filePath2 = TestTools::getFullTestImagePath("ometiff", "Multifile/multifile-Z1.ome.tiff");
	std::string filePath3 = TestTools::getFullTestImagePath("ometiff", "Subresolutions/Leica-2.ome.tiff");
	libtiff::TIFF* tiff = files.getOrOpen(filePath1);
	ASSERT_TRUE(tiff != nullptr);
	EXPECT_EQ(files.getNumberOfOpenFiles(), 1);
	EXPECT_EQ(files.getOpenFileCounter(), 1);
	EXPECT_EQ(files.getOrOpen(filePath1), tiff);
	EXPECT_EQ(files.getNumberOfOpenFiles(), 1);
	EXPECT_EQ(files.getOpenFileCounter(), 1);
	tiff = files.getOrOpen(filePath1);
	ASSERT_TRUE(tiff != nullptr);
	EXPECT_EQ(files.getNumberOfOpenFiles(), 1);
	EXPECT_EQ(files.getOpenFileCounter(), 1);
	EXPECT_EQ(files.getOrOpen(filePath1), tiff);
	EXPECT_EQ(files.getNumberOfOpenFiles(), 1);
	EXPECT_EQ(files.getOpenFileCounter(), 1);
	tiff = files.getOrOpen(filePath1);
	ASSERT_TRUE(tiff != nullptr);
	EXPECT_EQ(files.getNumberOfOpenFiles(), 1);
	EXPECT_EQ(files.getOpenFileCounter(), 1);
	EXPECT_EQ(files.getOrOpen(filePath1), tiff);
	EXPECT_EQ(files.getNumberOfOpenFiles(), 1);
	EXPECT_EQ(files.getOpenFileCounter(), 1);
    files.close(filePath1);
	EXPECT_EQ(files.getNumberOfOpenFiles(), 0);
	EXPECT_EQ(files.getOpenFileCounter(), 0);
	files.closeAll();
	EXPECT_EQ(files.getNumberOfOpenFiles(), 0);
	EXPECT_EQ(files.getOpenFileCounter(), 0);
	tiff = files.getOrOpen(filePath1);
	ASSERT_TRUE(tiff != nullptr);
	EXPECT_EQ(files.getNumberOfOpenFiles(), 1);
	EXPECT_EQ(files.getOpenFileCounter(), 1);
	tiff = files.getOrOpen(filePath2);
	ASSERT_TRUE(tiff != nullptr);
	EXPECT_EQ(files.getNumberOfOpenFiles(), 2);
	EXPECT_EQ(files.getOpenFileCounter(), 2);
	tiff = files.getOrOpen(filePath3);
	ASSERT_TRUE(tiff != nullptr);
	EXPECT_EQ(files.getNumberOfOpenFiles(), 3);
	EXPECT_EQ(files.getOpenFileCounter(), 3);
	files.close(filePath1);
	EXPECT_EQ(files.getNumberOfOpenFiles(), 2);
	EXPECT_EQ(files.getOpenFileCounter(), 2);
	files.closeAll();
	EXPECT_EQ(files.getNumberOfOpenFiles(), 0);
	EXPECT_EQ(files.getOpenFileCounter(), 0);
}

TEST_F(OTImageDriverTests, readBlockSingleTile) {
	std::string filePath = TestTools::getFullTestImagePath("ometiff", "Subresolutions/Leica-1.ome.tiff");
	std::string testFilePath = TestTools::getFullTestImagePath("ometiff", "Tests/Leica-1.ome.tiff - Series 1 (1, x=21504, y=15360, w=512, h=512).png");
	slideio::ometiff::OTImageDriver driver;
	std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
	ASSERT_TRUE(slide != nullptr);
	const int numScenes = slide->getNumScenes();
	ASSERT_EQ(numScenes, 2);
    std::shared_ptr<CVScene> scene = slide->getSceneByName("Image:1");
	cv::Rect sceneRect = scene->getRect();
	EXPECT_EQ(sceneRect, cv::Rect(0, 0, 36832, 38432));
	cv::Rect rect = { 21504, 15360, 512, 512 };
	std::vector<int> channels = { 0, 1, 2 };
	cv::Mat raster;
	scene->read4DBlockChannels(rect, channels, cv::Range(0, 1), cv::Range(0, 1), raster);
	EXPECT_FALSE(raster.empty());
	cv::Mat testRaster;
	ImageTools::readGDALImage(testFilePath, testRaster);
	EXPECT_TRUE(TestTools::compareRastersEx(raster, testRaster));
}

TEST_F(OTImageDriverTests, readBlock3Chnls) {
	std::string filePath = TestTools::getFullTestImagePath("ometiff", "Subresolutions/Leica-1.ome.tiff");
	std::string testFilePath = TestTools::getFullTestImagePath("ometiff", "Tests/Leica-1.ome.tiff - Series 1 (1, x=24000, y=18000, w=2000, h=1000).png");
	std::string testFileDownsampledPath = TestTools::getFullTestImagePath("ometiff", "Tests/Leica-1.ome.tiff - Series 1 (4, x=24000, y=18000, w=2000, h=1000).png");
	slideio::ometiff::OTImageDriver driver;
	std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
	ASSERT_TRUE(slide != nullptr);
	const int numScenes = slide->getNumScenes();
	ASSERT_EQ(numScenes, 2);
	std::shared_ptr<CVScene> scene = slide->getSceneByName("Image:1");
	// Read original scale
	cv::Rect rect = { 24000, 18000, 2000, 1000 };
	std::vector<int> channels = {};
	cv::Mat raster;
	scene->read4DBlockChannels(rect, channels, cv::Range(0, 1), cv::Range(0, 1), raster);
	EXPECT_FALSE(raster.empty());
	cv::Mat testRaster;
	ImageTools::readGDALImage(testFilePath, testRaster);
	double sim = ImageTools::computeSimilarity2(raster, testRaster);
	EXPECT_GT(sim, 0.99);
	// Read downsampled scale (4x)
	cv::Size size = { rect.size().width / 4, rect.size().height / 4 };
	scene->readResampled4DBlockChannels(rect, size, channels, cv::Range(0, 1), cv::Range(0, 1), raster);
	ImageTools::readGDALImage(testFileDownsampledPath, testRaster);
	sim = ImageTools::computeSimilarity2(raster, testRaster);
	EXPECT_GT(sim, 0.99);
	// Read downsampled scale single channel
	scene->readResampled4DBlockChannels(rect, size, { 0 }, cv::Range(0, 1), cv::Range(0, 1), raster);
	cv::Mat channelRaster;
	cv::extractChannel(testRaster, channelRaster, 0);
	sim = ImageTools::computeSimilarity2(raster, channelRaster);
	EXPECT_GT(sim, 0.99);
}

TEST_F(OTImageDriverTests, readBlock3ChnlsBGR) {
	std::string filePath = TestTools::getFullTestImagePath("ometiff", "Subresolutions/Leica-1.ome.tiff");
	std::string testFilePath = TestTools::getFullTestImagePath("ometiff", "Tests/Leica-1.ome.tiff - Series 1 (1, x=24000, y=18000, w=2000, h=1000).png");
	std::string testFileDownsampledPath = TestTools::getFullTestImagePath("ometiff", "Tests/Leica-1.ome.tiff - Series 1 (4, x=24000, y=18000, w=2000, h=1000).png");
	slideio::ometiff::OTImageDriver driver;
	std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
	ASSERT_TRUE(slide != nullptr);
	const int numScenes = slide->getNumScenes();
	ASSERT_EQ(numScenes, 2);
	std::shared_ptr<CVScene> scene = slide->getSceneByName("Image:1");
	// Read original scale
	cv::Rect rect = { 24000, 18000, 2000, 1000 };
	std::vector<int> channels = {2,1,0};
	cv::Mat raster;
	scene->read4DBlockChannels(rect, channels, cv::Range(0, 1), cv::Range(0, 1), raster);
	EXPECT_FALSE(raster.empty());
	cv::Mat testRaster;
	ImageTools::readGDALImage(testFilePath, testRaster);
	cv::cvtColor(testRaster, testRaster, cv::COLOR_RGB2BGR);
	double sim = ImageTools::computeSimilarity2(raster, testRaster);
	EXPECT_GT(sim, 0.99);
	// Read downsampled scale (4x)
	cv::Size size = { rect.size().width / 4, rect.size().height / 4 };
	scene->readResampled4DBlockChannels(rect, size, channels, cv::Range(0, 1), cv::Range(0, 1), raster);
	ImageTools::readGDALImage(testFileDownsampledPath, testRaster);
	cv::cvtColor(testRaster, testRaster, cv::COLOR_RGB2BGR);
	sim = ImageTools::computeSimilarity2(raster, testRaster);
	EXPECT_GT(sim, 0.99);
	// Read downsampled scale single channel
	scene->readResampled4DBlockChannels(rect, size, { 0 }, cv::Range(0, 1), cv::Range(0, 1), raster);
	cv::Mat channelRaster;
	cv::extractChannel(testRaster, channelRaster, 0);
	sim = ImageTools::computeSimilarity2(raster, channelRaster);
	EXPECT_GT(sim, 0.99);
}

TEST_F(OTImageDriverTests, readBlockZStackChannels) {
	std::string filePath = TestTools::getFullTestImagePath("ometiff", "Subresolutions/retina_large.ome.tiff");
	std::string testFilePathCh1 = TestTools::getFullTestImagePath("ometiff", "Tests/retina_large.ome-page32-channel-0.tif");
	std::string testFilePathCh2 = TestTools::getFullTestImagePath("ometiff", "Tests/retina_large.ome-page32-channel-1.tif");
	slideio::ometiff::OTImageDriver driver;
	std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
	ASSERT_TRUE(slide != nullptr);
	const int numScenes = slide->getNumScenes();
	ASSERT_EQ(numScenes, 2);
	std::shared_ptr<CVScene> scene = slide->getSceneByName("retina_large.ims Resolution Level 1");
	ASSERT_TRUE(scene != nullptr);
	// Read original scale
	cv::Rect rect = { 1000, 300, 1000, 500 };
	std::vector<int> channels = {};
	cv::Mat raster;
	scene->read4DBlockChannels(rect, channels, cv::Range(32, 33), cv::Range(0, 1), raster);
	EXPECT_FALSE(raster.empty());
	cv::Mat channelRaster;
	cv::extractChannel(raster, channelRaster, 0);
	
	cv::Mat testRaster;
	ImageTools::readGDALImage(testFilePathCh1, testRaster);
	cv::Mat region = testRaster(rect);
	double sim = ImageTools::computeSimilarity2(channelRaster, region);
	EXPECT_GT(sim, 0.99);

	cv::extractChannel(raster, channelRaster, 1);
	ImageTools::readGDALImage(testFilePathCh2, testRaster);
	region = testRaster(rect);
	sim = ImageTools::computeSimilarity2(channelRaster, region);
	EXPECT_GT(sim, 0.99);
}

TEST_F(OTImageDriverTests, readBlockZStackSlices) {
	std::string filePath = TestTools::getFullTestImagePath("ometiff", "Subresolutions/retina_large.ome.tiff");
	std::vector<std::string> sliceFiles = {
		TestTools::getFullTestImagePath("ometiff", "Tests/page_24.tif"),
		TestTools::getFullTestImagePath("ometiff", "Tests/page_25.tif"),
		TestTools::getFullTestImagePath("ometiff", "Tests/page_26.tif"),
		TestTools::getFullTestImagePath("ometiff", "Tests/page_27.tif"),
		TestTools::getFullTestImagePath("ometiff", "Tests/page_28.tif"),
		TestTools::getFullTestImagePath("ometiff", "Tests/page_29.tif"),
		TestTools::getFullTestImagePath("ometiff", "Tests/page_30.tif"),
    	TestTools::getFullTestImagePath("ometiff", "Tests/page_31.tif"),
	};
	slideio::ometiff::OTImageDriver driver;
	std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
	ASSERT_TRUE(slide != nullptr);
	const int numScenes = slide->getNumScenes();
	ASSERT_EQ(numScenes, 2);
	std::shared_ptr<CVScene> scene = slide->getSceneByName("retina_large.ims Resolution Level 1");
	ASSERT_TRUE(scene != nullptr);
	// Read original scale
	cv::Rect rect = { 1000, 300, 1000, 500 };
	cv::Mat raster;
	scene->read4DBlockChannels(rect, {0}, cv::Range(24, 32), cv::Range(0, 1), raster);
	EXPECT_FALSE(raster.empty());
	EXPECT_EQ(3, raster.dims);
	EXPECT_EQ(8, raster.size[2]);
	cv::Range xRange = cv::Range(0, raster.size[0]);
	cv::Range yRange = cv::Range(0, raster.size[1]);
	std::vector<cv::Range> ranges = { cv::Range::all(), cv::Range::all(), cv::Range::all() };
	for(int slice=0; slice<8; slice++) {
		ranges[2] = cv::Range(slice, slice + 1);
		cv::Mat sliceRaster = raster(ranges).clone();
		cv::Mat planeSlice = sliceRaster.reshape(0, {0,0});
		cv::Mat testRaster;
		ImageTools::readGDALImage(sliceFiles[slice], testRaster);
		cv::Mat region = testRaster(rect);
		double sim = ImageTools::computeSimilarity2(planeSlice, region);
		EXPECT_GT(sim, 0.99);
	}
}

TEST_F(OTImageDriverTests, readBlock4DMultifile) {
	std::string filePath = TestTools::getFullTestImagePath("ometiff", "tubhiswt-4D/tubhiswt_C0_TP0.ome.tif");
	std::vector<std::string> sliceFiles = {
		TestTools::getFullTestImagePath("ometiff", "Tests/tubhiswt4D-C0-T20-Z3.tif"),
		TestTools::getFullTestImagePath("ometiff", "Tests/tubhiswt4D-C0-T20-Z4.tif"),
		TestTools::getFullTestImagePath("ometiff", "Tests/tubhiswt4D-C0-T20-Z5.tif"),
		TestTools::getFullTestImagePath("ometiff", "Tests/tubhiswt4D-C0-T20-Z6.tif"),
	};
	std::vector<std::string> frameFiles = {
		TestTools::getFullTestImagePath("ometiff", "Tests/tubhiswt4D-C1-T20-Z5.tif"),
		TestTools::getFullTestImagePath("ometiff", "Tests/tubhiswt4D-C1-T21-Z5.tif"),
		TestTools::getFullTestImagePath("ometiff", "Tests/tubhiswt4D-C1-T22-Z5.tif"),
		TestTools::getFullTestImagePath("ometiff", "Tests/tubhiswt4D-C1-T23-Z5.tif"),
	};
	OTImageDriver driver;
	std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
	ASSERT_TRUE(slide != nullptr);
	const int numScenes = slide->getNumScenes();
	ASSERT_EQ(numScenes, 1);
	std::shared_ptr<CVScene> scene = slide->getSceneByName("tubhiswt");
	ASSERT_TRUE(scene != nullptr);
	EXPECT_EQ(2, scene->getNumChannels());
	EXPECT_EQ(10, scene->getNumZSlices());
	EXPECT_EQ(43, scene->getNumTFrames());
	cv::Rect rectScene = scene->getRect();
	EXPECT_EQ(rectScene, cv::Rect(0, 0, 512, 512));
	cv::Rect rectROI = { 128, 128, 256, 256 };
	cv::Mat raster;
	const int firstSlice = 3;
	const int lastSlice = 6;
	const int slices = lastSlice - firstSlice + 1;
	scene->read4DBlockChannels(rectROI, { 0 }, cv::Range(firstSlice, lastSlice+1), cv::Range(20, 21), raster);
	EXPECT_FALSE(raster.empty());
	EXPECT_EQ(3, raster.dims);
	EXPECT_EQ(slices, raster.size[2]);
	cv::Range xRange = cv::Range(0, raster.size[0]);
	cv::Range yRange = cv::Range(0, raster.size[1]);
	std::vector<cv::Range> ranges = { cv::Range::all(), cv::Range::all(), cv::Range::all() };
	for (int slice = 0; slice < slices; slice++) {
		ranges[2] = cv::Range(slice, slice + 1);
		cv::Mat sliceRaster = raster(ranges).clone();
		cv::Mat planeSlice = sliceRaster.reshape(0, { 0,0 });
		cv::Mat testRaster;
		ImageTools::readGDALImage(sliceFiles[slice], testRaster);
		cv::Mat region = testRaster(rectROI);
		double sim = ImageTools::computeSimilarity2(planeSlice, region);
		EXPECT_GT(sim, 0.99);
	}

	const int firstFrame = 20;
	const int lastFrame = 23;
	const int frames = lastFrame - firstFrame + 1;
	scene->read4DBlockChannels(rectROI, { 1 }, cv::Range(5, 6), cv::Range(firstFrame, lastFrame+1), raster);
	EXPECT_FALSE(raster.empty());
	EXPECT_EQ(3, raster.dims);
	EXPECT_EQ(frames, raster.size[2]);
	xRange = cv::Range(0, raster.size[0]);
	yRange = cv::Range(0, raster.size[1]);
	ranges = { cv::Range::all(), cv::Range::all(), cv::Range::all() };
	for (int frame = 0; frame < frames; frame++) {
		ranges[2] = cv::Range(frame, frame + 1);
		cv::Mat sliceRaster = raster(ranges).clone();
		cv::Mat planeSlice = sliceRaster.reshape(0, { 0,0 });
		cv::Mat testRaster;
		ImageTools::readGDALImage(frameFiles[frame], testRaster);
		cv::Mat region = testRaster(rectROI);
		double sim = ImageTools::computeSimilarity2(planeSlice, region);
		EXPECT_GT(sim, 0.99);
	}
}

TEST_F(OTImageDriverTests, magnification) {
	std::vector<std::tuple<std::string, int, double>> testCases = {
		{TestTools::getFullTestImagePath("ometiff", "tubhiswt-4D/tubhiswt_C0_TP0.ome.tif"), 0, 100.},
		{TestTools::getFullTestImagePath("ometiff", "Subresolutions/Leica-1.ome.tiff"), 0, 0.60833},
		{TestTools::getFullTestImagePath("ometiff", "Subresolutions/Leica-1.ome.tiff"), 1, 20.},
		{TestTools::getFullTestImagePath("ometiff", "Subresolutions/Leica-2.ome.tiff"), 0, 0.60833},
		{TestTools::getFullTestImagePath("ometiff", "Subresolutions/Leica-2.ome.tiff"), 1, 40.},
	};
	for (const auto& testCase : testCases) {
		std::string filePath = std::get<0>(testCase);
		int sceneIndex = std::get<1>(testCase);
		double expectedMagnification = std::get<2>(testCase);
		slideio::ometiff::OTImageDriver driver;
		std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
		ASSERT_TRUE(slide != nullptr);
		const int numScenes = slide->getNumScenes();
		ASSERT_GT(numScenes, 0);
		std::shared_ptr<CVScene> scene = slide->getScene(sceneIndex);
		ASSERT_TRUE(scene != nullptr);
		EXPECT_DOUBLE_EQ(scene->getMagnification(), expectedMagnification);
	}
}

TEST_F(OTImageDriverTests, metadata) {
	std::vector<std::tuple<std::string, int, double>> testCases = {
		{TestTools::getFullTestImagePath("ometiff", "tubhiswt-4D/tubhiswt_C0_TP0.ome.tif"), 0, 100.},
		{TestTools::getFullTestImagePath("ometiff", "Subresolutions/Leica-1.ome.tiff"), 0, 0.60833},
		{TestTools::getFullTestImagePath("ometiff", "Subresolutions/Leica-1.ome.tiff"), 1, 20.},
		{TestTools::getFullTestImagePath("ometiff", "Subresolutions/Leica-2.ome.tiff"), 0, 0.60833},
		{TestTools::getFullTestImagePath("ometiff", "Subresolutions/Leica-2.ome.tiff"), 1, 40.},
	};
	for (const auto& testCase : testCases) {
		std::string filePath = std::get<0>(testCase);
		slideio::ometiff::OTImageDriver driver;
		std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
		ASSERT_TRUE(slide != nullptr);
		EXPECT_EQ(slide->getMetadataFormat(), slideio::MetadataFormat::XML);
		auto metadata = slide->getRawMetadata();
		EXPECT_FALSE(metadata.empty());
		EXPECT_TRUE(TestTools::starts_with(metadata, "<?xml"));
		EXPECT_EQ(slide->getScene(0)->getMetadataFormat(), slideio::MetadataFormat::None);
	}
}