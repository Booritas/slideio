#include <gtest/gtest.h>
#include <opencv2/highgui.hpp>

#include "slideio/converter/convertersvstools.hpp"
#include "tests/testlib/testtools.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/imagetools/tempfile.hpp"


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

TEST(ConverterSVSTools, createDescription)
{
	std::string imagePath = TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs");
    CVSlidePtr slide = slideio::ImageDriverManager::openSlide(imagePath,"SVS");
    CVScenePtr scene = slide->getScene(0);
    std::string description = slideio::ConverterSVSTools::createDescription(scene);
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
    cv::Size tileSize(256,256);
	slideio::TempFile tiff("tiff");
	TIFFKeeperPtr file(new slideio::TIFFKeeper(tiff.getPath().string(), false));
	slideio::ConverterSVSTools::createZoomLevel(file, 0, scene, tileSize);
	file->closeTiffFile();
	std::vector<slideio::TiffDirectory> dirs;
	slideio::TiffTools::scanFile(tiff.getPath().string(), dirs);

	cv::Mat target;
	slideio::ImageTools::readGDALImage(tiff.getPath().string(), target);
	double similarity = slideio::ImageTools::computeSimilarity(source, target(sourceRect));
	EXPECT_GT(similarity, 0.99);
	// namedWindow( "Display window", cv::WINDOW_AUTOSIZE );// Create a window for display.
 //    cv::imshow( "Display window", target(sourceRect) );                   // Show our image inside it.
 //    cv::waitKey(0);
}

TEST(ConverterSVSTools, createZoomLevelColor)
{
	std::string imagePath = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
	CVSlidePtr slide = slideio::ImageDriverManager::openSlide(imagePath, "GDAL");
	CVScenePtr scene = slide->getScene(0);
	cv::Rect sourceRect = scene->getRect();
	cv::Mat source;
	slideio::ImageTools::readGDALImage(imagePath, source);
	cv::Size tileSize(256, 256);
	slideio::TempFile tiff("tiff");
	TIFFKeeperPtr file(new slideio::TIFFKeeper(tiff.getPath().string(), false));
	slideio::ConverterSVSTools::createZoomLevel(file, 0, scene, tileSize);
	file->closeTiffFile();
	std::vector<slideio::TiffDirectory> dirs;
	slideio::TiffTools::scanFile(tiff.getPath().string(), dirs);

	cv::Mat target;
	slideio::ImageTools::readGDALImage(tiff.getPath().string(), target);
	double similarity = slideio::ImageTools::computeSimilarity(source, target(sourceRect));
	EXPECT_GT(similarity, 0.99);
	namedWindow( "Display window", cv::WINDOW_AUTOSIZE );	// Create a window for display.
    cv::imshow( "Display window", target(sourceRect) );   // Show our image inside it.
    cv::waitKey(0);
}
