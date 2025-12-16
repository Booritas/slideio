#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include "slideio/converter/converter.hpp"
#include "slideio/slideio/slideio.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/converter/converterparameters.hpp"
#include "slideio/core/tools/tempfile.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"

#include <filesystem>


TEST(Converter, convertGDALJpeg) {
	std::string path = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
	SlidePtr slide = slideio::openSlide(path, "GDAL");
	ScenePtr scene = slide->getScene(0);
	auto sceneRect = scene->getRect();
	int sceneWidth = std::get<2>(sceneRect);
	int sceneHeight = std::get<3>(sceneRect);
	ASSERT_TRUE(scene.get() != nullptr);

	slideio::TempFile tmp("svs");
	std::string outputPath = tmp.getPath().string();
	if(std::filesystem::exists(outputPath)) {
		std::filesystem::remove(outputPath);
	}
    slideio::converter::SVSJpegConverterParameters parameters;
	parameters.setQuality(99);
	slideio::converter::convertScene(scene, parameters, outputPath);
	SlidePtr svsSlide = slideio::openSlide(outputPath, "SVS");
	ScenePtr svsScene = svsSlide->getScene(0);
	auto svsRect = svsScene->getRect();
	EXPECT_EQ(sceneWidth, std::get<2>(svsRect));
	EXPECT_EQ(sceneHeight, std::get<3>(svsRect));
	int dataSize = sceneHeight * sceneWidth * scene->getNumChannels();
	std::vector<uint8_t> svsBuffer(dataSize);
	svsScene->readBlock(sceneRect, svsBuffer.data(), svsBuffer.size());
	std::vector<uint8_t> gdalBuffer(dataSize);
	scene->readBlock(sceneRect, gdalBuffer.data(), gdalBuffer.size());
	cv::Mat svsImage(sceneHeight, sceneWidth, CV_8UC3, svsBuffer.data());
	cv::Mat gdalImage(sceneHeight, sceneWidth, CV_8UC3, gdalBuffer.data());
    double sim = slideio::ImageTools::computeSimilarity(svsImage, gdalImage);
	EXPECT_LE(0.99, sim);
}

TEST(Converter, convertGDALJp2K)
{
	std::string path = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
	SlidePtr slide = slideio::openSlide(path, "GDAL");
	ScenePtr scene = slide->getScene(0);
	auto sceneRect = scene->getRect();
	int sceneWidth = std::get<2>(sceneRect);
	int sceneHeight = std::get<3>(sceneRect);
	ASSERT_TRUE(scene.get() != nullptr);

	std::string outputPath = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.svs");
	if (std::filesystem::exists(outputPath)) {
		std::filesystem::remove(outputPath);
	}
	slideio::converter::SVSJp2KConverterParameters parameters;
	slideio::converter::convertScene(scene, parameters, outputPath);
	SlidePtr svsSlide = slideio::openSlide(outputPath, "SVS");
	ScenePtr svsScene = svsSlide->getScene(0);
	EXPECT_EQ(scene->getChannelDataType(0), svsScene->getChannelDataType(0));
	auto svsRect = svsScene->getRect();
	EXPECT_EQ(sceneWidth, std::get<2>(svsRect));
	EXPECT_EQ(sceneHeight, std::get<3>(svsRect));
	int dataSize = sceneHeight * sceneWidth * scene->getNumChannels();
	std::vector<uint8_t> svsBuffer(dataSize);
	svsScene->readBlock(sceneRect, svsBuffer.data(), svsBuffer.size());
	std::vector<uint8_t> gdalBuffer(dataSize);
	scene->readBlock(sceneRect, gdalBuffer.data(), gdalBuffer.size());
	cv::Mat svsImage(sceneHeight, sceneWidth, CV_8UC3, svsBuffer.data());
	cv::Mat gdalImage(sceneHeight, sceneWidth, CV_8UC3, gdalBuffer.data());
	double sim = slideio::ImageTools::computeSimilarity(svsImage, gdalImage);
	EXPECT_LE(0.99, sim);
}

TEST(Converter, nullScene)
{
	std::string outputPath = TestTools::getTestImagePath("gdal", "test.svs");
	slideio::converter::SVSJpegConverterParameters parameters;
	parameters.setQuality(99);
	ASSERT_THROW(slideio::converter::convertScene(nullptr, parameters, outputPath), slideio::RuntimeError);
}

TEST(Converter, unsupportedDriver)
{
	std::string path = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
	SlidePtr slide = slideio::openSlide(path, "GDAL");
	ScenePtr scene = slide->getScene(0);
	ASSERT_TRUE(scene.get() != nullptr);
	
	slideio::converter::SVSJpegConverterParameters parameters;
	parameters.setQuality(99);
	std::string outputPath = TestTools::getTestImagePath("gdal", "test.svs");
	ASSERT_THROW(slideio::converter::convertScene(scene, parameters, outputPath), slideio::RuntimeError);
}

TEST(Converter, outputPathExists)
{
	std::string path = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
	SlidePtr slide = slideio::openSlide(path, "GDAL");
	ScenePtr scene = slide->getScene(0);
	ASSERT_TRUE(scene.get() != nullptr);
	
	slideio::converter::SVSJpegConverterParameters parameters;
	parameters.setQuality(99);
	ASSERT_THROW(slideio::converter::convertScene(scene, parameters, path), slideio::RuntimeError);
}

TEST(Converter, fromMultipleScenes)
{
	if (!TestTools::isFullTestEnabled())
	{
		GTEST_SKIP() << "Skip private test because full dataset is not enabled";
	}
	std::string path = TestTools::getFullTestImagePath("czi", "jxr-rgb-5scenes.czi");
	SlidePtr slide = slideio::openSlide(path);
    ScenePtr scene = slide->getScene(0);
	ASSERT_TRUE(scene.get() != nullptr);

	const int x = 1000;
	const int y = 2000;
	const int width = 400;
	const int height = 300;
	const int numChannels = scene->getNumChannels();
	const int dataTypeSize = slideio::Tools::dataTypeSize(scene->getChannelDataType(0));
	const int rasterSize = width * height * numChannels * dataTypeSize;

    const std::tuple<int, int, int, int> block = { x,y,width,height };
	std::vector<uint8_t> buffer(rasterSize);
	scene->readBlock(block, buffer.data(), buffer.size());
	
	slideio::converter::SVSJpegConverterParameters parameters;
	parameters.setQuality(90);
	slideio::Rect rect = { x,y, width, height};
	parameters.setRect(rect);
    const slideio::TempFile tmp("svs");
    const std::string outputPath = tmp.getPath().string();
	if (std::filesystem::exists(outputPath)) {
		std::filesystem::remove(outputPath);
	}
	slideio::converter::convertScene(scene, parameters, outputPath);
	SlidePtr outputSlide = slideio::openSlide(outputPath);
	ASSERT_TRUE(outputSlide != nullptr);
	ScenePtr outputScene = outputSlide->getScene(0);
	EXPECT_EQ(scene->getChannelDataType(0), outputScene->getChannelDataType(0));
	ASSERT_TRUE(outputScene != nullptr);
	const auto outputRect = outputScene->getRect();
	ASSERT_EQ(std::get<0>(outputRect), 0);
	ASSERT_EQ(std::get<1>(outputRect), 0);
	ASSERT_EQ(std::get<2>(outputRect), width);
	ASSERT_EQ(std::get<3>(outputRect), height);
	std::vector<uint8_t> outputBuffer(rasterSize);
	outputScene->readBlock(outputRect, outputBuffer.data(), outputBuffer.size());
	cv::Mat inputImage(height, width, CV_8UC3, buffer.data());
	cv::Mat outputImage(height, width, CV_8UC3, outputBuffer.data());
	double sim = slideio::ImageTools::computeSimilarity(inputImage, outputImage);
	EXPECT_LE(0.998, sim);

}


TEST(Converter, from3DScene)
{
	if (!TestTools::isFullTestEnabled())
	{
		GTEST_SKIP() << "Skip private test because full dataset is not enabled";
	}
	std::string path = TestTools::getFullTestImagePath("czi", "pJP31mCherry.czi");
	SlidePtr slide = slideio::openSlide(path);
	ScenePtr scene = slide->getScene(0);
	ASSERT_TRUE(scene.get() != nullptr);

	const int x = 100;
	const int y = 100;
	const int width = 300;
	const int height = 300;
	const int numChannels = scene->getNumChannels();
	const int dataTypeSize = slideio::Tools::dataTypeSize(scene->getChannelDataType(0));
	const int rasterSize = width * height * numChannels * dataTypeSize;
	const int slice = 6;
	const int frame = 0;

	constexpr std::tuple<int, int> sliceRange(slice, slice + 1);
	constexpr std::tuple<int, int> frameRange(frame, frame + 1);
	constexpr std::tuple<int, int, int, int> block = { x,y,width,height };
	std::vector<uint8_t> buffer(rasterSize);

	scene->read4DBlock(block, sliceRange, frameRange, buffer.data(), buffer.size());

	slideio::converter::SVSJpegConverterParameters parameters;
	parameters.setSliceRange(cv::Range(slice, slice+1));
	parameters.setTFrameRange(cv::Range(frame, frame+1));
	parameters.setQuality(90);
	slideio::Rect rect = { x,y, width, height };
	parameters.setRect(rect);
	const slideio::TempFile tmp("svs");
	const std::string outputPath = tmp.getPath().string();
	if (std::filesystem::exists(outputPath)) {
		std::filesystem::remove(outputPath);
	}
	slideio::converter::convertScene(scene, parameters, outputPath);
	SlidePtr outputSlide = slideio::openSlide(outputPath);
	ASSERT_TRUE(outputSlide != nullptr);
	ScenePtr outputScene = outputSlide->getScene(0);
	ASSERT_TRUE(outputScene != nullptr);
	EXPECT_EQ(scene->getChannelDataType(0), outputScene->getChannelDataType(0));
	const auto outputRect = outputScene->getRect();
	ASSERT_EQ(std::get<0>(outputRect), 0);
	ASSERT_EQ(std::get<1>(outputRect), 0);
	ASSERT_EQ(std::get<2>(outputRect), width);
	ASSERT_EQ(std::get<3>(outputRect), height);
	std::vector<uint8_t> outputBuffer(rasterSize);
	outputScene->readBlock(outputRect, outputBuffer.data(), outputBuffer.size());
	cv::Mat inputImage(height, width, CV_8UC3, buffer.data());
	cv::Mat outputImage(height, width, CV_8UC3, outputBuffer.data());
	double sim = slideio::ImageTools::computeSimilarity(inputImage, outputImage);
	EXPECT_LE(0.999, sim);

}

TEST(Converter, jpeg2k4channelsScene)
{
	if (!TestTools::isFullTestEnabled())
	{
		GTEST_SKIP() << "Skip private test because full dataset is not enabled";
	}
	std::string path = TestTools::getFullTestImagePath("czi", "jxr-16bit-4chnls.czi");
	SlidePtr slide = slideio::openSlide(path);
	ScenePtr scene = slide->getScene(0);
	ASSERT_TRUE(scene.get() != nullptr);

	const int x = 1000;
	const int y = 1500;
	const int width = 700;
	const int height = 600;
	const int numChannels = scene->getNumChannels();
	const int dataTypeSize = slideio::Tools::dataTypeSize(scene->getChannelDataType(0));
	const int rasterSize = width * height * numChannels * dataTypeSize;
	const int slice = 0;
	const int frame = 0;

	constexpr std::tuple<int, int> sliceRange(slice, slice + 1);
	constexpr std::tuple<int, int> frameRange(frame, frame + 1);
	constexpr std::tuple<int, int, int, int> block = { x,y,width,height };
	std::vector<uint8_t> buffer(rasterSize);

	scene->read4DBlock(block, sliceRange, frameRange, buffer.data(), buffer.size());

	slideio::converter::SVSJp2KConverterParameters parameters;
	parameters.setSliceRange(cv::Range(slice, slice+1));
	parameters.setTFrameRange(cv::Range(frame, frame+1));
	slideio::Rect rect = { x,y, width, height };
	parameters.setRect(rect);
	const slideio::TempFile tmp("svs");
	const std::string outputPath = tmp.getPath().string();
	if (std::filesystem::exists(outputPath)) {
		std::filesystem::remove(outputPath);
	}
	slideio::converter::convertScene(scene, parameters, outputPath);
	SlidePtr outputSlide = slideio::openSlide(outputPath);
	ASSERT_TRUE(outputSlide != nullptr);
	ScenePtr outputScene = outputSlide->getScene(0);
	ASSERT_TRUE(outputScene != nullptr);
	EXPECT_EQ(scene->getChannelDataType(0), outputScene->getChannelDataType(0));
	const auto outputRect = outputScene->getRect();
	ASSERT_EQ(std::get<0>(outputRect), 0);
	ASSERT_EQ(std::get<1>(outputRect), 0);
	ASSERT_EQ(std::get<2>(outputRect), width);
	ASSERT_EQ(std::get<3>(outputRect), height);
	std::vector<uint8_t> outputBuffer(rasterSize);
	outputScene->readBlock(outputRect, outputBuffer.data(), outputBuffer.size());
	cv::Mat inputImage(height, width, CV_16UC4, buffer.data());
	cv::Mat outputImage(height, width, CV_16UC4, outputBuffer.data());
	double sim = slideio::ImageTools::computeSimilarity(inputImage, outputImage);
	EXPECT_LE(0.999, sim);

}


TEST(Converter, invalidRegions)
{
	if (!TestTools::isFullTestEnabled())
	{
		GTEST_SKIP() << "Skip private test because full dataset is not enabled";
	}
	std::string path = TestTools::getFullTestImagePath("czi", "jxr-rgb-5scenes.czi");
	SlidePtr slide = slideio::openSlide(path);
	ScenePtr scene = slide->getScene(0);
	ASSERT_TRUE(scene.get() != nullptr);
    std::tuple<int, int, int, int> sceneRect = scene->getRect();
	slideio::converter::SVSJpegConverterParameters parameters;

	int sx = 11;
	int sy = 4;
	const int width = sx * parameters.getTileWidth();
	const int height = sy * parameters.getTileHeight();
	const int x = std::get<2>(sceneRect) - width;
	const int y = std::get<3>(sceneRect) - height;
	const int numChannels = scene->getNumChannels();
	const int dataTypeSize = slideio::Tools::dataTypeSize(scene->getChannelDataType(0));
	const int rasterSize = width * height * numChannels * dataTypeSize;

	const std::tuple<int, int, int, int> block = { x,y,width,height };
	std::vector<uint8_t> buffer(rasterSize);
	scene->readBlock(block, buffer.data(), buffer.size());

	parameters.setQuality(90);
	slideio::Rect rect = { x,y, width, height };
	parameters.setRect(rect);
	const slideio::TempFile tmp("svs");
	const std::string outputPath = tmp.getPath().string();
	if (std::filesystem::exists(outputPath)) {
		std::filesystem::remove(outputPath);
	}
	slideio::converter::convertScene(scene, parameters, outputPath);
	SlidePtr outputSlide = slideio::openSlide(outputPath);
	ASSERT_TRUE(outputSlide != nullptr);
	ScenePtr outputScene = outputSlide->getScene(0);
	ASSERT_TRUE(outputScene != nullptr);
	EXPECT_EQ(scene->getChannelDataType(0), outputScene->getChannelDataType(0));
	const auto outputRect = outputScene->getRect();
	ASSERT_EQ(std::get<0>(outputRect), 0);
	ASSERT_EQ(std::get<1>(outputRect), 0);
	ASSERT_EQ(std::get<2>(outputRect), width);
	ASSERT_EQ(std::get<3>(outputRect), height);
	std::vector<uint8_t> outputBuffer(rasterSize);
	outputScene->readBlock(outputRect, outputBuffer.data(), outputBuffer.size());
	cv::Mat inputImage(height, width, CV_8UC3, buffer.data());
	cv::Mat outputImage(height, width, CV_8UC3, outputBuffer.data());
	double sim = slideio::ImageTools::computeSimilarity(inputImage, outputImage);
	EXPECT_LE(0.999, sim);
}

TEST(Converter, jpeg2k)
{
	if (!TestTools::isFullTestEnabled())
	{
		GTEST_SKIP() << "Skip private test because full dataset is not enabled";
	}
	std::string path = TestTools::getFullTestImagePath("czi", "doughnut.czi");
	SlidePtr slide = slideio::openSlide(path);
	ScenePtr scene = slide->getScene(0);
	ASSERT_TRUE(scene.get() != nullptr);


	slideio::converter::SVSJp2KConverterParameters parameters;
	const slideio::TempFile tmp("svs");
	const std::string outputPath = tmp.getPath().string();
	if (std::filesystem::exists(outputPath)) {
		std::filesystem::remove(outputPath);
	}
	auto sceneRect = scene->getRect();
	int width = std::get<2>(sceneRect);
	int height = std::get<3>(sceneRect);
	int numChannels = scene->getNumChannels();
	int dataTypeSize = slideio::Tools::dataTypeSize(scene->getChannelDataType(0));
	int rasterSize = width * height * numChannels * dataTypeSize;
	std::tuple<int,int,int,int> block(0,0,width,height);
	std::vector<uint8_t> buffer(rasterSize);
	scene->readBlock(block, buffer.data(), buffer.size());

	slideio::converter::convertScene(scene, parameters, outputPath);
	SlidePtr outputSlide = slideio::openSlide(outputPath);
	ASSERT_TRUE(outputSlide != nullptr);
	ScenePtr outputScene = outputSlide->getScene(0);
	ASSERT_TRUE(outputScene != nullptr);
	EXPECT_EQ(scene->getChannelDataType(0), outputScene->getChannelDataType(0));

	const auto outputRect = outputScene->getRect();
	ASSERT_EQ(std::get<0>(outputRect), 0);
	ASSERT_EQ(std::get<1>(outputRect), 0);
	ASSERT_EQ(std::get<2>(outputRect), width);
	ASSERT_EQ(std::get<3>(outputRect), height);
	std::vector<uint8_t> outputBuffer(rasterSize);
	outputScene->readBlock(outputRect, outputBuffer.data(), outputBuffer.size());
	cv::Mat inputImage(height, width, CV_16UC1, buffer.data());
	cv::Mat outputImage(height, width, CV_16UC1, outputBuffer.data());
	double sim = slideio::ImageTools::computeSimilarity(inputImage, outputImage);
	//TestTools::showRaster(outputImage);
	EXPECT_LE(0.99, sim);

}

TEST(Converter, jpeg2kBorderTiles)
{
	std::string path = TestTools::getTestImagePath("czi", "08_18_2018_enc_1001_633.czi");
	SlidePtr slide = slideio::openSlide(path);
	ScenePtr scene = slide->getScene(0);
	ASSERT_TRUE(scene.get() != nullptr);


	slideio::converter::SVSJp2KConverterParameters parameters;
	const slideio::TempFile tmp("svs");
	const std::string outputPath = tmp.getPath().string();
	if (std::filesystem::exists(outputPath)) {
		std::filesystem::remove(outputPath);
	}
	auto sceneRect = scene->getRect();
	int width = std::get<2>(sceneRect);
	int height = std::get<3>(sceneRect);
	int numChannels = scene->getNumChannels();
	int dataTypeSize = slideio::Tools::dataTypeSize(scene->getChannelDataType(0));
	int rasterSize = width * height * numChannels * dataTypeSize;
	std::tuple<int, int, int, int> block(0, 0, width, height);
	std::vector<uint8_t> buffer(rasterSize);
	scene->readBlock(block, buffer.data(), buffer.size());

	slideio::converter::convertScene(scene, parameters, outputPath);
	SlidePtr outputSlide = slideio::openSlide(outputPath);
	ASSERT_TRUE(outputSlide != nullptr);
	ScenePtr outputScene = outputSlide->getScene(0);
	ASSERT_TRUE(outputScene != nullptr);
	EXPECT_EQ(scene->getChannelDataType(0), outputScene->getChannelDataType(0));

	const auto outputRect = outputScene->getRect();
	ASSERT_EQ(std::get<0>(outputRect), 0);
	ASSERT_EQ(std::get<1>(outputRect), 0);
	ASSERT_EQ(std::get<2>(outputRect), width);
	ASSERT_EQ(std::get<3>(outputRect), height);
	std::vector<uint8_t> outputBuffer(rasterSize);
	outputScene->readBlock(outputRect, outputBuffer.data(), outputBuffer.size());
	cv::Mat inputImage(height, width, CV_16UC1, buffer.data());
	cv::Mat outputImage(height, width, CV_16UC1, outputBuffer.data());
	double sim = slideio::ImageTools::computeSimilarity(inputImage, outputImage);
	//TestTools::showRaster(outputImage);
	EXPECT_LE(0.999, sim);

}

TEST(Converter, metadata)
{
	std::string path = TestTools::getTestImagePath("scn", "Leica-Fluorescence-1.scn");
	SlidePtr slide = slideio::openSlide(path);
	ScenePtr scene = slide->getScene(0);
	ASSERT_TRUE(scene.get() != nullptr);

	const int x = 100;
	const int y = 100;
	const int width = 300;
	const int height = 300;
	const int numChannels = scene->getNumChannels();
	const int dataTypeSize = slideio::Tools::dataTypeSize(scene->getChannelDataType(0));
	const int rasterSize = width * height * numChannels * dataTypeSize;
	constexpr std::tuple<int, int, int, int> block = { x,y,width,height };
	std::vector<uint8_t> buffer(rasterSize);

	scene->readBlock(block, buffer.data(), buffer.size());

	slideio::converter::SVSJpegConverterParameters parameters;
	parameters.setQuality(90);
	slideio::Rect rect = { x,y, width, height };
	parameters.setRect(rect);
	const slideio::TempFile tmp("svs");
	const std::string outputPath = tmp.getPath().string();
	if (std::filesystem::exists(outputPath)) {
		std::filesystem::remove(outputPath);
	}
	slideio::converter::convertScene(scene, parameters, outputPath);
	SlidePtr outputSlide = slideio::openSlide(outputPath);
	ASSERT_TRUE(outputSlide != nullptr);
	ScenePtr outputScene = outputSlide->getScene(0);
	ASSERT_TRUE(outputScene != nullptr);
	EXPECT_EQ(scene->getChannelDataType(0), outputScene->getChannelDataType(0));

	const auto outputRect = outputScene->getRect();
	ASSERT_EQ(std::get<0>(outputRect), 0);
	ASSERT_EQ(std::get<1>(outputRect), 0);
	ASSERT_EQ(std::get<2>(outputRect), width);
	ASSERT_EQ(std::get<3>(outputRect), height);
	std::vector<uint8_t> outputBuffer(rasterSize);
	outputScene->readBlock(outputRect, outputBuffer.data(), outputBuffer.size());
	cv::Mat inputImage(height, width, CV_8UC3, buffer.data());
	cv::Mat outputImage(height, width, CV_8UC3, outputBuffer.data());
	double sim = slideio::ImageTools::computeSimilarity(inputImage, outputImage);
	EXPECT_LE(0.999, sim);
	double originMagnificaton = scene->getMagnification();
	double outputMagnification = outputScene->getMagnification();
	EXPECT_DOUBLE_EQ(originMagnificaton, outputMagnification);
	std::tuple<double, double> originRes = scene->getResolution();
	std::tuple<double, double> outputRes = outputScene->getResolution();
	EXPECT_DOUBLE_EQ(std::get<0>(originRes), std::get<0>(outputRes));
	EXPECT_DOUBLE_EQ(std::get<1>(originRes), std::get<1>(outputRes));
}

TEST(Converter, intData)
{
	std::string path = TestTools::getTestImagePath("zvi", "Zeiss-1-Merged.zvi");
	SlidePtr slide = slideio::openSlide(path);
	ScenePtr scene = slide->getScene(0);
	ASSERT_TRUE(scene.get() != nullptr);

	const int x = 100;
	const int y = 100;
	const int width = 300;
	const int height = 300;
	const int numChannels = scene->getNumChannels();
	const int dataTypeSize = slideio::Tools::dataTypeSize(scene->getChannelDataType(0));
	const int rasterSize = width * height * numChannels * dataTypeSize;
	constexpr std::tuple<int, int, int, int> block = { x,y,width,height };
	std::vector<uint8_t> buffer(rasterSize);

	scene->readBlock(block, buffer.data(), buffer.size());

	slideio::converter::SVSJp2KConverterParameters parameters;
	parameters.setCompressionRate(5);
	slideio::Rect rect = { x,y, width, height };
	parameters.setRect(rect);
	const slideio::TempFile tmp("svs");
	const std::string outputPath = tmp.getPath().string();
	if (std::filesystem::exists(outputPath)) {
		std::filesystem::remove(outputPath);
	}
	slideio::converter::convertScene(scene, parameters, outputPath);
	SlidePtr outputSlide = slideio::openSlide(outputPath);
	ASSERT_TRUE(outputSlide != nullptr);
	ScenePtr outputScene = outputSlide->getScene(0);
	ASSERT_TRUE(outputScene != nullptr);
	EXPECT_EQ(scene->getChannelDataType(0), outputScene->getChannelDataType(0));

	const auto outputRect = outputScene->getRect();
	ASSERT_EQ(std::get<0>(outputRect), 0);
	ASSERT_EQ(std::get<1>(outputRect), 0);
	ASSERT_EQ(std::get<2>(outputRect), width);
	ASSERT_EQ(std::get<3>(outputRect), height);
	std::vector<uint8_t> outputBuffer(rasterSize);
	outputScene->readBlock(outputRect, outputBuffer.data(), outputBuffer.size());
	cv::Mat inputImage(height, width, CV_16SC3, buffer.data());
	cv::Mat outputImage(height, width, CV_16SC3, outputBuffer.data());
	double sim = slideio::ImageTools::computeSimilarity(inputImage, outputImage);
	EXPECT_LE(0.999, sim);
	double originMagnificaton = scene->getMagnification();
	double outputMagnification = outputScene->getMagnification();
	EXPECT_DOUBLE_EQ(originMagnificaton, outputMagnification);
	std::tuple<double, double> originRes = scene->getResolution();
	std::tuple<double, double> outputRes = outputScene->getResolution();
	EXPECT_DOUBLE_EQ(std::get<0>(originRes), std::get<0>(outputRes));
	EXPECT_DOUBLE_EQ(std::get<1>(originRes), std::get<1>(outputRes));
}


TEST(Converter, createSVS8bitGray)
{
	std::string imagePath = TestTools::getTestImagePath("gdal", "img_2448x2448_1x8bit_SRC_GRAY_ducks.png");
	std::shared_ptr<slideio::Slide> slide = slideio::openSlide(imagePath, "GDAL");
	ASSERT_NE(slide, nullptr);
	const int numScenes = slide->getNumScenes();
	ASSERT_EQ(numScenes, 1);
	std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
	slideio::TempFile svs("svs");
	slideio::converter::SVSJpegConverterParameters parameters;
	parameters.setQuality(99);
	parameters.setTileWidth(256);
	parameters.setTileHeight(256);
	slideio::converter::convertScene(scene, parameters, svs.getPath().string());

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

TEST(Converter, createSVS8bitColor)
{
	std::string imagePath = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
	std::shared_ptr<slideio::Slide> slide = slideio::openSlide(imagePath, "GDAL");
	ASSERT_NE(slide, nullptr);
	const int numScenes = slide->getNumScenes();
	ASSERT_EQ(numScenes, 1);
	std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
	slideio::TempFile svs("svs");
	slideio::converter::SVSJpegConverterParameters parameters;
	parameters.setQuality(99);
	parameters.setTileWidth(256);
	parameters.setTileHeight(256);
	slideio::converter::convertScene(scene, parameters, svs.getPath().string());

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
