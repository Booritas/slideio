#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include <string>
#include <tinyxml2.h>
#include <opencv2/imgproc.hpp>
#include <slideio/slideio/imagedrivermanager.hpp>
#include "slideio/core/tools/tools.hpp"
#include "slideio/drivers/ome-tiff/otimagedriver.hpp"
#include "slideio/drivers/ome-tiff/otscene.hpp"
#include "slideio/imagetools/smallimage.hpp"
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
		ImageDriverManager::setLogLevel("ERROR");
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
	// The suffixes above all drop the dot INSIDE the extension (".ometiff"),
	// which no sub-pattern could match anyway. These keep a well-formed
	// ".tiff" and instead let "ome" run into the end of the file name, so they
	// fail only if a sub-pattern is missing its leading dot -- as
	// "*ome.tiff" was, matching every file whose name happened to end in
	// "ome" plus a tiff extension.
	const std::string namesEndingInOme[] = { "/projects/genome.tiff", "/projects/myome.tif",
	                                         "/projects/genome.tf2", "/projects/genome.btf" };
	for (const std::string& filePath : namesEndingInOme) {
		EXPECT_FALSE(driver.canOpenFile(filePath)) << filePath;
	}
}

TEST_F(OTImageDriverTests, openSlide) {
	std::string filePath = TestTools::getTestImagePath("ometiff", "Multifile/multifile-Z1.ome.tiff");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
	auto slide = slideio::openSlide(filePath, "OMETIFF");
	ASSERT_TRUE(slide != nullptr);
	slide = slideio::openSlide(filePath, "AUTO");
	ASSERT_TRUE(slide != nullptr);
}

TEST_F(OTImageDriverTests, readInt8Scene) {
	std::string filePath = TestTools::getTestImagePath("ometiff", "4D-Series/4D-series.ome.tiff");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
	auto slide = slideio::openSlide(filePath, "OMETIFF");
	ASSERT_TRUE(slide != nullptr);
	slide = slideio::openSlide(filePath, "AUTO");
	ASSERT_TRUE(slide != nullptr);
	ASSERT_EQ(slide->getNumScenes(), 1);
	auto scene = slide->getScene(0);
	ASSERT_TRUE(scene != nullptr);
	auto rect = scene->getRect();
	const int width = 439;
	const int height = 167;
	EXPECT_EQ(std::get<2>(rect), width);
	EXPECT_EQ(std::get<3>(rect), height);
	EXPECT_EQ(scene->getChannelDataType(0), DataType::DT_Int8);
	EXPECT_EQ(scene->getNumChannels(), 1);
	EXPECT_EQ(scene->getNumZSlices(), 5);
	EXPECT_EQ(scene->getNumTFrames(), 7);
	EXPECT_EQ(scene->getCompression(), Compression::Uncompressed);
	const int coef = 2;
	std::tuple<int,int> size = {width * coef, height * coef};
	const int sz = std::get<0>(size) * std::get<1>(size);
	std::vector<uint8_t> buffer(sz);;
	EXPECT_NO_THROW(scene->readResampledBlock(rect, size, buffer.data(), buffer.size()));
}

TEST_F(OTImageDriverTests, openMultifileSlide) {
    std::string filePath = TestTools::getTestImagePath("ometiff", "Multifile/multifile-Z1.ome.tiff");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
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
	std::string filePath = TestTools::getTestImagePath("ometiff", "Multifile2/multifile-Z1.ome.tiff");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
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

TEST_F(OTImageDriverTests, getDriverId)
{
	std::string filePath = TestTools::getTestImagePath("ometiff", "Subresolutions/Leica-2.ome.tiff");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
	auto slide = slideio::openSlide(filePath, "AUTO");
	ASSERT_TRUE(slide);
	const int numScenes = slide->getNumScenes();
	EXPECT_EQ(5, numScenes);
	for (int iScene=0; iScene<numScenes; ++iScene) {
		std::shared_ptr<slideio::CVScene> scene = slide->getScene(iScene)->getCVScene();
		EXPECT_TRUE(scene.get() != nullptr);
		EXPECT_EQ(iScene, scene->getSceneIndex());
		EXPECT_EQ(filePath, scene->getFilePath());
		EXPECT_EQ("OMETIFF", scene->getDriverId());
	}
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

    std::string filePath = TestTools::getTestImagePath("ometiff", "Subresolutions/Leica-2.ome.tiff");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
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
		EXPECT_EQ(scene->getChannelDataType(0), sceneInfo.dt);
		EXPECT_NEAR(scene->getResolution().x, sceneInfo.res.x, 1.e-12);
		EXPECT_NEAR(scene->getResolution().y, sceneInfo.res.y, 1.e-12);
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
	std::string filePath = TestTools::getTestImagePath("ometiff", "Subresolutions/retina_large.ome.tiff");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
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
		EXPECT_NEAR(scene->getResolution().x, sceneInfo.res.x, 1.e-12);
		EXPECT_NEAR(scene->getResolution().y, sceneInfo.res.y, 1.e-12);
		EXPECT_EQ(scene->getChannelDataType(0), sceneInfo.dt);
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
	std::string filePath1 = TestTools::getTestImagePath("ometiff", "Subresolutions/retina_large.ome.tiff");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath1);
	std::string filePath2 = TestTools::getTestImagePath("ometiff", "Multifile/multifile-Z1.ome.tiff");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath2);
	std::string filePath3 = TestTools::getTestImagePath("ometiff", "Subresolutions/Leica-2.ome.tiff");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath3);
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
	std::string filePath = TestTools::getTestImagePath("ometiff", "Subresolutions/Leica-1.ome.tiff");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
	std::string testFilePath = TestTools::getTestImagePath("ometiff", "Tests/Leica-1.ome.tiff - Series 1 (1, x=21504, y=15360, w=512, h=512).png");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath);
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
	ImageTools::readSmallImageRaster(testFilePath, testRaster);
	EXPECT_TRUE(TestTools::compareRastersEx(raster, testRaster));
}

TEST_F(OTImageDriverTests, readBlock3Chnls) {
	std::string filePath = TestTools::getTestImagePath("ometiff", "Subresolutions/Leica-1.ome.tiff");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
	std::string testFilePath = TestTools::getTestImagePath("ometiff", "Tests/Leica-1.ome.tiff - Series 1 (1, x=24000, y=18000, w=2000, h=1000).png");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath);
	std::string testFileDownsampledPath = TestTools::getTestImagePath("ometiff", "Tests/Leica-1.ome.tiff - Series 1 (4, x=24000, y=18000, w=2000, h=1000).png");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(testFileDownsampledPath);
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
	ImageTools::readSmallImageRaster(testFilePath, testRaster);
	double sim = ImageTools::computeSimilarity2(raster, testRaster);
	EXPECT_GT(sim, 0.99);
	// Read downsampled scale (4x)
	cv::Size size = { rect.size().width / 4, rect.size().height / 4 };
	scene->readResampled4DBlockChannels(rect, size, channels, cv::Range(0, 1), cv::Range(0, 1), raster);
	ImageTools::readSmallImageRaster(testFileDownsampledPath, testRaster);
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
	std::string filePath = TestTools::getTestImagePath("ometiff", "Subresolutions/Leica-1.ome.tiff");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
	std::string testFilePath = TestTools::getTestImagePath("ometiff", "Tests/Leica-1.ome.tiff - Series 1 (1, x=24000, y=18000, w=2000, h=1000).png");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath);
	std::string testFileDownsampledPath = TestTools::getTestImagePath("ometiff", "Tests/Leica-1.ome.tiff - Series 1 (4, x=24000, y=18000, w=2000, h=1000).png");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(testFileDownsampledPath);
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
	ImageTools::readSmallImageRaster(testFilePath, testRaster);
	cv::cvtColor(testRaster, testRaster, cv::COLOR_RGB2BGR);
	double sim = ImageTools::computeSimilarity2(raster, testRaster);
	EXPECT_GT(sim, 0.99);
	// Read downsampled scale (4x)
	cv::Size size = { rect.size().width / 4, rect.size().height / 4 };
	scene->readResampled4DBlockChannels(rect, size, channels, cv::Range(0, 1), cv::Range(0, 1), raster);
	ImageTools::readSmallImageRaster(testFileDownsampledPath, testRaster);
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
	std::string filePath = TestTools::getTestImagePath("ometiff", "Subresolutions/retina_large.ome.tiff");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
	std::string testFilePathCh1 = TestTools::getTestImagePath("ometiff", "Tests/retina_large.ome-page32-channel-0.tif");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePathCh1);
	std::string testFilePathCh2 = TestTools::getTestImagePath("ometiff", "Tests/retina_large.ome-page32-channel-1.tif");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePathCh2);
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
	ImageTools::readSmallImageRaster(testFilePathCh1, testRaster);
	cv::Mat region = testRaster(rect);
	double sim = ImageTools::computeSimilarity2(channelRaster, region);
	EXPECT_GT(sim, 0.99);

	cv::extractChannel(raster, channelRaster, 1);
	ImageTools::readSmallImageRaster(testFilePathCh2, testRaster);
	region = testRaster(rect);
	sim = ImageTools::computeSimilarity2(channelRaster, region);
	EXPECT_GT(sim, 0.99);
}

TEST_F(OTImageDriverTests, readBlockZStackSlices) {
	std::string filePath = TestTools::getTestImagePath("ometiff", "Subresolutions/retina_large.ome.tiff");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
	std::vector<std::string> sliceFiles = {
		TestTools::getTestImagePath("ometiff", "Tests/page_24.tif"),
		TestTools::getTestImagePath("ometiff", "Tests/page_25.tif"),
		TestTools::getTestImagePath("ometiff", "Tests/page_26.tif"),
		TestTools::getTestImagePath("ometiff", "Tests/page_27.tif"),
		TestTools::getTestImagePath("ometiff", "Tests/page_28.tif"),
		TestTools::getTestImagePath("ometiff", "Tests/page_29.tif"),
		TestTools::getTestImagePath("ometiff", "Tests/page_30.tif"),
    	TestTools::getTestImagePath("ometiff", "Tests/page_31.tif"),
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
		ImageTools::readSmallImageRaster(sliceFiles[slice], testRaster);
		cv::Mat region = testRaster(rect);
		double sim = ImageTools::computeSimilarity2(planeSlice, region);
		EXPECT_GT(sim, 0.99);
	}
}

TEST_F(OTImageDriverTests, readBlock4DMultifile) {
	std::string filePath = TestTools::getTestImagePath("ometiff", "tubhiswt-4D/tubhiswt_C0_TP0.ome.tif");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
	std::vector<std::string> sliceFiles = {
		TestTools::getTestImagePath("ometiff", "Tests/tubhiswt4D-C0-T20-Z3.tif"),
		TestTools::getTestImagePath("ometiff", "Tests/tubhiswt4D-C0-T20-Z4.tif"),
		TestTools::getTestImagePath("ometiff", "Tests/tubhiswt4D-C0-T20-Z5.tif"),
		TestTools::getTestImagePath("ometiff", "Tests/tubhiswt4D-C0-T20-Z6.tif"),
	};
	std::vector<std::string> frameFiles = {
		TestTools::getTestImagePath("ometiff", "Tests/tubhiswt4D-C1-T20-Z5.tif"),
		TestTools::getTestImagePath("ometiff", "Tests/tubhiswt4D-C1-T21-Z5.tif"),
		TestTools::getTestImagePath("ometiff", "Tests/tubhiswt4D-C1-T22-Z5.tif"),
		TestTools::getTestImagePath("ometiff", "Tests/tubhiswt4D-C1-T23-Z5.tif"),
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
		ImageTools::readSmallImageRaster(sliceFiles[slice], testRaster);
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
		ImageTools::readSmallImageRaster(frameFiles[frame], testRaster);
		cv::Mat region = testRaster(rectROI);
		double sim = ImageTools::computeSimilarity2(planeSlice, region);
		EXPECT_GT(sim, 0.99);
	}
}

TEST_F(OTImageDriverTests, magnification) {
	std::vector<std::tuple<std::string, int, double>> testCases = {
		{TestTools::getTestImagePath("ometiff", "tubhiswt-4D/tubhiswt_C0_TP0.ome.tif"), 0, 100.},
		{TestTools::getTestImagePath("ometiff", "Subresolutions/Leica-1.ome.tiff"), 0, 0.60833},
		{TestTools::getTestImagePath("ometiff", "Subresolutions/Leica-1.ome.tiff"), 1, 20.},
		{TestTools::getTestImagePath("ometiff", "Subresolutions/Leica-2.ome.tiff"), 0, 0.60833},
		{TestTools::getTestImagePath("ometiff", "Subresolutions/Leica-2.ome.tiff"), 1, 40.},
	};
	// Paths live inside the tuples, so the per-statement guard cannot see them.
	for (const auto& testCase : testCases) {
		SLIDEIO_SKIP_IF_IMAGE_MISSING(std::get<0>(testCase));
	}
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
		{TestTools::getTestImagePath("ometiff", "tubhiswt-4D/tubhiswt_C0_TP0.ome.tif"), 0, 100.},
		{TestTools::getTestImagePath("ometiff", "Subresolutions/Leica-1.ome.tiff"), 0, 0.60833},
		{TestTools::getTestImagePath("ometiff", "Subresolutions/Leica-1.ome.tiff"), 1, 20.},
		{TestTools::getTestImagePath("ometiff", "Subresolutions/Leica-2.ome.tiff"), 0, 0.60833},
		{TestTools::getTestImagePath("ometiff", "Subresolutions/Leica-2.ome.tiff"), 1, 40.},
	};
	// Paths live inside the tuples, so the per-statement guard cannot see them.
	for (const auto& testCase : testCases) {
		SLIDEIO_SKIP_IF_IMAGE_MISSING(std::get<0>(testCase));
	}
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

TEST_F(OTImageDriverTests, readBlockLargeFileLZW) {
	std::string filePath = TestTools::getTestImagePath("ometiff", "private/test.ome.tif");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
	std::string testFilePathCh1 = TestTools::getTestImagePath("ometiff", "Tests/test.ome.tif - USL-2023-53777-20 (1, x=16245, y=23321, w=1028, h=640).tif");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePathCh1);
	slideio::ometiff::OTImageDriver driver;
	std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
	ASSERT_TRUE(slide != nullptr);
	const int numScenes = slide->getNumScenes();
	ASSERT_EQ(numScenes, 1);
	std::shared_ptr<CVScene> scene = slide->getScene(0);
	ASSERT_TRUE(scene != nullptr);
	cv::Rect sceneRect = scene->getRect();
	EXPECT_EQ(sceneRect, cv::Rect(0, 0, 27136, 36160));
	const int numChannels = scene->getNumChannels();
	EXPECT_EQ(numChannels, 15);
	const std::string channelNames[] = {
		"DAPI",
		"CD8",
		"CD4",
		"CD11c",
		"CD68",
		"DAPI2",
		"CD11b",
		"PD-1",
		"CD56",
		"CD20",
		"DAPI3",
		"CD3",
		"CD14",
		"CD206",
		"CK"
	};
	std::string channelName;
	int channelIndex = 0;
	for (const std::string& name : channelNames) {
		auto compression = scene->getCompression();
		EXPECT_EQ(compression, Compression::LZW);
		channelName = scene->getChannelName(channelIndex++);
	}

	cv::Mat testRaster;
	ImageTools::openSmallImage(testFilePathCh1)->readImageStack(testRaster);

	// Read original scale
	cv::Rect rect = { 16245, 23321, 1028, 640 };
	cv::Mat raster;
	std::vector<int> channels = { 0 };
	cv::Mat channelRaster;
	for (int channelIndex=0; channelIndex<numChannels; ++channelIndex) {
		channels[0] = channelIndex;
		scene->read4DBlockChannels(rect, channels, cv::Range(0, 1), cv::Range(0, 1), raster);
		cv::extractChannel(testRaster, channelRaster, channelIndex);
	    double sim = ImageTools::computeSimilarity2(channelRaster, raster);
		EXPECT_GT(sim, 0.99);
	}

	cv::Size rasterSize = { rect.size().width / 4, rect.size().height / 4 };
	cv::Mat resizedChannel;
	for (int channelIndex = 0; channelIndex < numChannels; ++channelIndex) {
		channels[0] = channelIndex;
		scene->readResampledBlockChannels(rect, rasterSize, channels, raster);
		cv::extractChannel(testRaster, channelRaster, channelIndex);
		cv::resize(channelRaster, resizedChannel, rasterSize);
		double sim = ImageTools::computeSimilarity2(resizedChannel, raster);
		EXPECT_GT(sim, 0.99);
	}
}

TEST_F(OTImageDriverTests, sceneWithPixeltype) {
	std::string filePath = TestTools::getTestImagePath("ometiff", "Iron-Plate.ome.tiff");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
	std::string testFilePathCh1 = TestTools::getTestImagePath("ometiff", "Tests/Iron-Plate (1, x=144, y=146, w=258, h=175).tif");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePathCh1);
	slideio::ometiff::OTImageDriver driver;
	std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
	ASSERT_TRUE(slide != nullptr);
	const int numScenes = slide->getNumScenes();
	ASSERT_EQ(numScenes, 1);
	std::shared_ptr<CVScene> scene = slide->getScene(0);
	ASSERT_TRUE(scene != nullptr);
	auto numChannels = scene->getNumChannels();
	EXPECT_EQ(numChannels, 3);
	auto dataType = scene->getChannelDataType(0);
	EXPECT_EQ(dataType, DataType::DT_Byte);
	auto compression = scene->getCompression();
	EXPECT_EQ(compression, Compression::Uncompressed);
	cv::Rect blockRect = { 144, 146, 258, 175 };
	cv::Mat raster;
	scene->readBlock(blockRect, raster);
	EXPECT_FALSE(raster.empty());
	cv::Mat testRaster;
	ImageTools::openSmallImage(testFilePathCh1)->readImageStack(testRaster);
	double sim = ImageTools::computeSimilarity2(raster, testRaster);
	EXPECT_GT(sim, 0.99);
}

TEST_F(OTImageDriverTests, channelAttributes) {
    std::string filePath = TestTools::getTestImagePath("ometiff", "private/test.ome.tif");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    OTImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    auto scene = slide->getScene(0);
    const int numChannels = scene->getNumChannels();
    const slideio::Metadata& chanAttrs = scene->getChannelAttributes();
    ASSERT_EQ(static_cast<int>(chanAttrs.size()), numChannels);
    EXPECT_EQ(chanAttrs[0].size(), 6u);

    typedef std::tuple<std::string, std::vector<std::string>> AttInfo;
    std::vector<AttInfo> expectedAttNames = {
        {"ID", {"Channel:0:0", "Channel:0:1","Channel:0:2","Channel:0:3",
            "Channel:0:4","Channel:0:5", "Channel:0:6","Channel:0:7","Channel:0:8",
            "Channel:0:9","Channel:0:10","Channel:0:11", "Channel:0:12","Channel:0:13",
            "Channel:0:14","Channel:0:15"}},
        {"SamplesPerPixel", {"1","1","1","1","1",
            "1","1","1","1","1",
            "1","1","1","1","1"}},
        {"Name", {"DAPI", "CD8", "CD4", "CD11c", "CD68",
            "DAPI2", "CD11b", "PD-1", "CD56", "CD20",
            "DAPI3", "CD3", "CD14","CD206", "CK"}},
        {"Color", {"#FF0000FF", "#FFFF6600", "#FFFFCC00", "#FFCBFF00", "#FF65FF00",
            "#FF0000FF", "#FF00FF66", "#FF00FFCB", "#FF00CBFF", "#FF0066FF",
            "#FF0000FF", "#FF6500FF", "#FFCC00FF", "#FFFF00CB", "#FFFF0066"}},
        {"ContrastMethod", {"Fluorescence", "Fluorescence", "Fluorescence", "Fluorescence", "Fluorescence",
            "Fluorescence", "Fluorescence", "Fluorescence", "Fluorescence", "Fluorescence",
            "Fluorescence", "Fluorescence", "Fluorescence", "Fluorescence", "Fluorescence"}},
        {"EmissionWavelength", {"440", "371", "392", "413", "434",
            "440", "476", "497", "518", "539",
            "440", "581","602", "623", "645"}}
    };

    for (const AttInfo& info : expectedAttNames) {
        const std::string& expectedAttName = std::get<0>(info);
        const std::vector<std::string>& expectedValues = std::get<1>(info);
        for (int channel = 0; channel < numChannels && channel < static_cast<int>(expectedValues.size()); ++channel) {
            ASSERT_TRUE(chanAttrs[channel].contains(expectedAttName))
                << "Channel " << channel << " missing attribute " << expectedAttName;
            EXPECT_EQ(chanAttrs[channel][expectedAttName].asString(), expectedValues[channel])
                << "Channel " << channel << " attribute " << expectedAttName;
        }
    }
}

TEST_F(OTImageDriverTests, readBlockBigEndian) {
	std::string filePath = TestTools::getTestImagePath("ometiff", "private/ULT-2020-111-014-1.ome.tif");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
	std::string testFilePaths[] = {	TestTools::getTestImagePath("ometiff", "Tests/ULT-2020-111-014_1 (1, x=4375, y=39330, w=1153, h=743).tif"),
	                                TestTools::getTestImagePath("ometiff", "Tests/ULT-2020-111-014_1 (1, x=28333, y=36086, w=1099, h=760).tif")
	};
	OTImageDriver driver;
	std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
	ASSERT_TRUE(slide != nullptr);
	const int numScenes = slide->getNumScenes();
	ASSERT_EQ(numScenes, 1);
	std::shared_ptr<CVScene> scene = slide->getScene(0);
	ASSERT_TRUE(scene != nullptr);
	cv::Rect sceneRect = scene->getRect();
	EXPECT_EQ(sceneRect, cv::Rect(0, 0, 53527, 57198));
	const int numChannels = scene->getNumChannels();
	EXPECT_EQ(numChannels, 19);
	const std::string channelNames[] = {
		"DAPI",
		"AF488",
		"AF555",
		"Cy5",
		"Cy7",
		"DAPI2",
		"CD8",
		"CD163",
		"CD3",
		"FoxP3",
		"DAPI3",
		"CK",
		"CD68",
		"PD-L1",
		"CD20",
        "DAPI4",
        "BLUE",
		"GREEN",
		"RED"
	};
	std::string channelName;
	int channelIndex = 0;
	for (const std::string& name : channelNames) {
		auto compression = scene->getCompression();
		EXPECT_EQ(compression, Compression::LZW);
		channelName = scene->getChannelName(channelIndex++);
	}

	cv::Rect rects[] = {
		 { 4375, 39330, 1153, 743 },
		 { 28333, 36086, 1099, 760 }
    };	
	for (int iPath = 0; iPath < 2; ++iPath) {
		std::string path = testFilePaths[iPath];
		cv::Rect rect = rects[iPath];
		cv::Mat testRaster;
		ImageTools::openSmallImage(path)->readImageStack(testRaster);

		// Read original scale
		cv::Mat raster;
		std::vector<int> channels = { 0 };
		cv::Mat channelRaster;
		for (int channelIndex = 0; channelIndex < numChannels; ++channelIndex) {
			channels[0] = channelIndex;
			scene->read4DBlockChannels(rect, channels, cv::Range(0, 1), cv::Range(0, 1), raster);
			cv::extractChannel(testRaster, channelRaster, channelIndex);
			double sim = ImageTools::computeSimilarity2(channelRaster, raster);
			EXPECT_GT(sim, 0.99);
		}

		cv::Size rasterSize = { rect.size().width / 4, rect.size().height / 4 };
		cv::Mat resizedChannel;
		for (int channelIndex = 0; channelIndex < numChannels; ++channelIndex) {
			channels[0] = channelIndex;
			scene->readResampledBlockChannels(rect, rasterSize, channels, raster);
			cv::extractChannel(testRaster, channelRaster, channelIndex);
			cv::resize(channelRaster, resizedChannel, rasterSize);
			double sim = ImageTools::computeSimilarity2(resizedChannel, raster);
			EXPECT_GT(sim, 0.9);
		}
	}
}

TEST_F(OTImageDriverTests, readLevelMatchesTheResampledSceneRead) {
	std::string filePath = TestTools::getTestImagePath("ometiff", "Subresolutions/Leica-2.ome.tiff");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
	slideio::ometiff::OTImageDriver driver;
	std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
	ASSERT_TRUE(slide != nullptr);
	std::shared_ptr<CVScene> scene = slide->getSceneByName("Image:4");
	ASSERT_TRUE(scene != nullptr);

    const int numLevels = scene->getNumZoomLevels();
    ASSERT_LE(2, numLevels);
    for (int level = 1; level < numLevels; ++level)
    {
        const slideio::LevelInfo* info = scene->getZoomLevelInfo(level);
        ASSERT_TRUE(info != nullptr) << "level " << level;
        const cv::Size levelSize(info->getSize().width, info->getSize().height);
        cv::Mat viaLevel, viaScene;
        scene->readResampledLevelBlockChannels(level, cv::Rect(cv::Point(0, 0), levelSize), levelSize, {}, viaLevel);
        scene->readResampledBlock(scene->getRect(), levelSize, viaScene);
        ASSERT_EQ(levelSize, viaLevel.size()) << "level " << level;
        EXPECT_LE(0.95, slideio::ImageTools::computeSimilarity2(viaLevel, viaScene)) << "level " << level;
    }
    // An overhanging rect is the ordinary edge tile and must come back background filled.
    const slideio::LevelInfo* last = scene->getZoomLevelInfo(numLevels - 1);
    const cv::Size lastSize(last->getSize().width, last->getSize().height);
    cv::Mat edge;
    ASSERT_NO_THROW(scene->readResampledLevelBlockChannels(
        numLevels - 1, cv::Rect(lastSize.width - 32, lastSize.height - 32, 128, 128),
        cv::Size(128, 128), {}, edge));
    EXPECT_EQ(cv::Size(128, 128), edge.size());
}

// The generic CVScene default clamps levelRect to the level and then re-derives a zoom
// index from the requested output size; when that re-derived index happens to agree with
// the level actually asked for, the default's output is byte-identical to a direct read of
// that other level. So reading level 0 resampled down to level 1's size must NOT come back
// identical to a native read of level 1 -- equality would mean level 0's read was actually
// served by level 1.
TEST_F(OTImageDriverTests, readLevelDoesNotReuseAdjacentLevel) {
	std::string filePath = TestTools::getTestImagePath("ometiff", "Subresolutions/Leica-2.ome.tiff");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
	slideio::ometiff::OTImageDriver driver;
	std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
	ASSERT_TRUE(slide != nullptr);
	std::shared_ptr<CVScene> scene = slide->getSceneByName("Image:4");
	ASSERT_TRUE(scene != nullptr);
	ASSERT_LE(2, scene->getNumZoomLevels());

	const slideio::LevelInfo* level0 = scene->getZoomLevelInfo(0);
	const slideio::LevelInfo* level1 = scene->getZoomLevelInfo(1);
	ASSERT_TRUE(level0 != nullptr);
	ASSERT_TRUE(level1 != nullptr);
	const cv::Size size0(level0->getSize().width, level0->getSize().height);
	const cv::Size size1(level1->getSize().width, level1->getSize().height);

	// Read level 0 resampled to level 1's size -- must be served from level 0 directly, not
	// silently reselected to level 1 by the generic base default.
	cv::Mat viaLevel0Resampled;
	scene->readResampledLevelBlockChannels(0, cv::Rect(cv::Point(0, 0), size0), size1, {}, viaLevel0Resampled);
	// Read level 1 natively: no resampling at all.
	cv::Mat viaLevel1Native;
	scene->readResampledLevelBlockChannels(1, cv::Rect(cv::Point(0, 0), size1), size1, {}, viaLevel1Native);

	ASSERT_EQ(size1, viaLevel0Resampled.size());
	ASSERT_EQ(size1, viaLevel1Native.size());
	// The two are independently encoded streams; equality means level 0's read was actually
	// served from level 1.
	EXPECT_GT(cv::norm(viaLevel0Resampled, viaLevel1Native, cv::NORM_INF), 0);
}
