#include <gtest/gtest.h>
//#include <opencv2/highgui.hpp>

#include "slideio/converter/convertersvstools.hpp"
#include "tests/testlib/testtools.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/converter/converter.hpp"
#include "slideio/converter/converterparameters.hpp"
#include "slideio/imagetools/tempfile.hpp"
#include "slideio/slideio/slideio.hpp"


TEST(ConverterSVSTools, checkSVSRequirements)
{
	struct TestImages
	{
		std::string path;
		std::string driver;
		bool jpeg;
		bool succeess;
	};
	TestImages tests[] = {
		{TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg"),
			"GDAL",
			true,
			true
		},
		{
			TestTools::getTestImagePath("gdal", "img_2448x2448_1x8bit_SRC_GRAY_ducks.png"),
			"GDAL",
			true,
			true
    	},
		{
			TestTools::getTestImagePath("dcm", "barre.dev/MR-MONO2-12-angio-an1"),
			"DCM",
			true,
			false
		},
		{
			TestTools::getTestImagePath("dcm", "barre.dev/MR-MONO2-12-angio-an1"),
			"DCM",
			false,
			true
		}
	};
    slideio::SVSJpegConverterParameters jpegParameters;
	slideio::SVSJp2KConverterParameters j2kParameters;
	for(auto test : tests) {
		CVSlidePtr slide = slideio::ImageDriverManager::openSlide(test.path, test.driver);
		CVScenePtr scene = slide->getScene(0);
		slideio::SVSConverterParameters& parameters = test.jpeg ? (slideio::SVSConverterParameters&)jpegParameters : (slideio::SVSConverterParameters&)j2kParameters;
		if(test.succeess) {
			EXPECT_NO_THROW(slideio::ConverterSVSTools::checkSVSRequirements(scene, parameters));
		}
		else {
			EXPECT_THROW(slideio::ConverterSVSTools::checkSVSRequirements(scene, parameters), slideio::RuntimeError);
		}
	}
}

TEST(ConverterSVSTools, createDescription)
{
	std::string imagePath = TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs");
    CVSlidePtr slide = slideio::ImageDriverManager::openSlide(imagePath,"SVS");
    CVScenePtr scene = slide->getScene(0);
	slideio::SVSJpegConverterParameters parameters;
	parameters.setQuality(99);
    std::string description = slideio::ConverterSVSTools::createDescription(scene, parameters);
	EXPECT_FALSE(description.empty());
	EXPECT_TRUE(description.find("SlideIO") >= 0);
	EXPECT_TRUE(description.find("2220x2967") > 0);
	EXPECT_TRUE(description.find("AppMag = 20") > 0);
}

TEST(ConverterSVSTools, createZoomLevelGray)
{
	std::string imagePath = TestTools::getTestImagePath("gdal", "img_2448x2448_1x8bit_SRC_GRAY_ducks.png");
	CVSlidePtr slide = slideio::ImageDriverManager::openSlide(imagePath, "GDAL");
	CVScenePtr scene = slide->getScene(0);
	cv::Rect sourceRect = scene->getRect();
	cv::Mat source;
	slideio::ImageTools::readGDALImage(imagePath, source);
	slideio::TempFile tiff("tiff");
	TIFFKeeperPtr file(new slideio::TIFFKeeper(tiff.getPath().string(), false));
	slideio::SVSJpegConverterParameters parameters;
	parameters.setQuality(99);
	parameters.setTileWidth(256);
	parameters.setTileHeight(256);
	slideio::ConverterSVSTools::createZoomLevel(file, 0, scene, parameters);
	file->closeTiffFile();
	std::vector<slideio::TiffDirectory> dirs;
	slideio::TiffTools::scanFile(tiff.getPath().string(), dirs);

	cv::Mat target;
	slideio::ImageTools::readGDALImage(tiff.getPath().string(), target);
	double similarity = slideio::ImageTools::computeSimilarity(source, target(sourceRect));
	EXPECT_GT(similarity, 0.99);
}

TEST(ConverterSVSTools, createZoomLevelColor)
{
	std::string imagePath = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
	CVSlidePtr slide = slideio::ImageDriverManager::openSlide(imagePath, "GDAL");
	CVScenePtr scene = slide->getScene(0);
	cv::Rect sourceRect = scene->getRect();
	cv::Mat source;
	slideio::ImageTools::readGDALImage(imagePath, source);
	cv::Size tileSize(256, 256);
	slideio::TempFile tiff("tiff");
	TIFFKeeperPtr file(new slideio::TIFFKeeper(tiff.getPath().string(), false));
	slideio::SVSJpegConverterParameters parameters;
	parameters.setQuality(99);
	parameters.setTileWidth(256);
	parameters.setTileHeight(256);
	slideio::ConverterSVSTools::createZoomLevel(file, 0, scene, parameters);
	file->closeTiffFile();
    cv::Mat target;
	slideio::ImageTools::readGDALImage(tiff.getPath().string(), target);
	double similarity = slideio::ImageTools::computeSimilarity(source, target(sourceRect));
	EXPECT_GT(similarity, 0.99);
}

TEST(ConverterSVSTools, createSVS8bitGray)
{
	std::string imagePath = TestTools::getTestImagePath("gdal", "img_2448x2448_1x8bit_SRC_GRAY_ducks.png");
	std::shared_ptr<slideio::Slide> slide = slideio::openSlide(imagePath, "GDAL");
	ASSERT_NE(slide, nullptr);
	const int numScenes = slide->getNumScenes();
	ASSERT_EQ(numScenes, 1);
	std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
	slideio::TempFile svs("svs");
	slideio::SVSJpegConverterParameters parameters;
	parameters.setQuality(99);
	parameters.setTileWidth(256);
	parameters.setTileHeight(256);
	slideio::convertScene(scene, parameters, svs.getPath().string());

	cv::Mat source;
	slideio::ImageTools::readGDALImage(imagePath, source);
	CVSlidePtr targetSlide = slideio::ImageDriverManager::openSlide(svs.getPath().string(), "SVS");
	CVScenePtr targetScene = targetSlide->getScene(0);
	cv::Rect sceneRect = targetScene->getRect();
	cv::Mat target;
	targetScene->readBlock(sceneRect, target);
	double similarity = slideio::ImageTools::computeSimilarity(source, target);
	EXPECT_GT(similarity, 0.99);
}

TEST(ConverterSVSTools, createSVS8bitColor)
{
	std::string imagePath = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
	std::shared_ptr<slideio::Slide> slide = slideio::openSlide(imagePath, "GDAL");
	ASSERT_NE(slide, nullptr);
	const int numScenes = slide->getNumScenes();
	ASSERT_EQ(numScenes, 1);
	std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
	slideio::TempFile svs("svs");
	slideio::SVSJpegConverterParameters parameters;
	parameters.setQuality(99);
	parameters.setTileWidth(256);
	parameters.setTileHeight(256);
	slideio::convertScene(scene, parameters, svs.getPath().string());

	cv::Mat source;
	slideio::ImageTools::readGDALImage(imagePath, source);
	CVSlidePtr targetSlide = slideio::ImageDriverManager::openSlide(svs.getPath().string(), "SVS");
	CVScenePtr targetScene = targetSlide->getScene(0);
	cv::Rect sceneRect = targetScene->getRect();
	cv::Mat target;
	targetScene->readBlock(sceneRect, target);
	double similarity = slideio::ImageTools::computeSimilarity(source, target);
	EXPECT_GT(similarity, 0.99);
}
