#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include "slideio/converter/converter.hpp"
#include "slideio/slideio/slideio.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/base/exceptions.hpp"
#include <boost/filesystem.hpp>
#include <opencv2/imgproc.hpp>
#include "slideio/transformer/transformation.hpp"
#include "slideio/transformer/transformer.hpp"

using namespace slideio;

TEST(ColorTransfomation, grayColor)
{
	std::string path = TestTools::getTestImagePath("gdal", "colors.png");
	std::shared_ptr<slideio::Slide> slide = openSlide(path, "AUTO");
	std::shared_ptr<slideio::Scene> originScene = slide->getScene(0);
	ColorTransformation transformation;
    transformation.setColorSpace(ColorSpace::GRAY);
	std::shared_ptr<slideio::Scene> transformedScene = transformScene(originScene, transformation);
	ASSERT_EQ(1, transformedScene->getNumChannels());
	std::tuple<int, int, int, int> rect = originScene->getRect();
	int width = std::get<2>(rect);
	int height = std::get<3>(rect);
	std::vector<unsigned char> buffer(width*height*3);
	originScene->readBlock(rect, buffer.data(), buffer.size());
	cv::Mat originImage(height, width, CV_8UC3, buffer.data());
	std::vector<uint8_t> transformedBuffer(width*height);
	transformedScene->readBlock(rect, transformedBuffer.data(), transformedBuffer.size());
	cv::Mat transformedImage(height, width, CV_8UC1, transformedBuffer.data());
	cv::Mat originGray;
    cv::cvtColor(originImage, originGray, cv::COLOR_RGB2GRAY);
	TestTools::compareRasters(originGray, transformedImage);
}

TEST(ColorTransfomation, colorspaces3channels)
{
	struct ColorSpaceItem
	{
		ColorSpace colorSpace;
		int cvTransformation;

	};
	ColorSpaceItem items[] = {
		{ ColorSpace::HSV, cv::COLOR_RGB2HSV},
		{ ColorSpace::HLS, cv::COLOR_RGB2HLS},
		{ ColorSpace::YUV, cv::COLOR_RGB2YUV},
		{ ColorSpace::YCbCr, cv::COLOR_RGB2YCrCb},
		{ ColorSpace::LUV, cv::COLOR_RGB2Luv},
		{ ColorSpace::LAB, cv::COLOR_RGB2Lab},
	    { ColorSpace::XYZ, cv::COLOR_RGB2XYZ}};
	

	std::string path = TestTools::getTestImagePath("gdal", "colors.png");
	std::shared_ptr<slideio::Slide> slide = openSlide(path, "AUTO");
	std::shared_ptr<slideio::Scene> originScene = slide->getScene(0);

	std::tuple<int, int, int, int> rect = originScene->getRect();
	const int width = std::get<2>(rect);
	const int height = std::get<3>(rect);
	const int bufferSize = width * height * 3;
	std::vector<unsigned char> buffer(bufferSize);
	originScene->readBlock(rect, buffer.data(), buffer.size());
	cv::Mat originImage(height, width, CV_8UC3, buffer.data());
	std::vector<uint8_t> transformedBuffer(bufferSize);

	ColorTransformation transformation;
	for(auto item: items)
	{
		ColorSpace colorSpace = item.colorSpace;
		int cvTransformation = item.cvTransformation;
	    transformation.setColorSpace(colorSpace);
        std::shared_ptr<slideio::Scene> transformedScene = transformScene(originScene, transformation);
        ASSERT_EQ(3, transformedScene->getNumChannels());
        transformedScene->readBlock(rect, transformedBuffer.data(), transformedBuffer.size());
        cv::Mat transformedImage(height, width, CV_8UC3, transformedBuffer.data());
        cv::Mat originGray;
        cv::cvtColor(originImage, originGray, cvTransformation);
        TestTools::compareRasters(originGray, transformedImage);
    }	
}

TEST(ColorTransfomation, grayColorLargeImage)
{
	std::string path = TestTools::getTestImagePath("svs", "JP2K-33003-1.svs");
	std::shared_ptr<slideio::Slide> slide = openSlide(path, "AUTO");
	std::shared_ptr<slideio::Scene> originScene = slide->getScene(0);
	ColorTransformation transformation;
	transformation.setColorSpace(ColorSpace::GRAY);
	std::shared_ptr<slideio::Scene> transformedScene = transformScene(originScene, transformation);
	ASSERT_EQ(1, transformedScene->getNumChannels());
	std::tuple<int, int, int, int> rect = originScene->getRect();
	int width = std::get<2>(rect);
	int height = std::get<3>(rect);
	int x = width / 3;
	int y = height / 3;
	int blockWidth = width / 3;
	int blockHeight = height / 3;
	std::tuple<int,int, int, int> blockRect(x,y,blockWidth, blockHeight);
	std::vector<unsigned char> buffer(blockWidth * blockHeight * 3);
	originScene->readBlock(blockRect, buffer.data(), buffer.size());
	cv::Mat originImage(blockHeight, blockWidth, CV_8UC3, buffer.data());
	std::vector<uint8_t> transformedBuffer(blockWidth * blockHeight);
	transformedScene->readBlock(blockRect, transformedBuffer.data(), transformedBuffer.size());
	cv::Mat transformedImage(blockHeight, blockWidth, CV_8UC1, transformedBuffer.data());
	cv::Mat originGray;
	cv::cvtColor(originImage, originGray, cv::COLOR_RGB2GRAY);
	TestTools::compareRasters(originGray, transformedImage);
}

TEST(ColorTransfomation, colorspaces3channelsLargeImage)
{
	struct ColorSpaceItem
	{
		ColorSpace colorSpace;
		int cvTransformation;

	};
	ColorSpaceItem items[] = {
		{ ColorSpace::HSV, cv::COLOR_RGB2HSV},
		{ ColorSpace::HLS, cv::COLOR_RGB2HLS},
		{ ColorSpace::YUV, cv::COLOR_RGB2YUV},
		{ ColorSpace::YCbCr, cv::COLOR_RGB2YCrCb},
		{ ColorSpace::LUV, cv::COLOR_RGB2Luv},
		{ ColorSpace::LAB, cv::COLOR_RGB2Lab},
		{ ColorSpace::XYZ, cv::COLOR_RGB2XYZ} };


	std::string path = TestTools::getTestImagePath("svs", "JP2K-33003-1.svs");
	std::shared_ptr<slideio::Slide> slide = openSlide(path, "AUTO");
	std::shared_ptr<slideio::Scene> originScene = slide->getScene(0);

	std::tuple<int, int, int, int> rect = originScene->getRect();
	const int width = std::get<2>(rect);
	const int height = std::get<3>(rect);
	const int x = width / 3;
	const int y = height / 3;
	const int blockWidth = width / 3;
	const int blockHeight = height / 3;
	const std::tuple<int, int, int, int> blockRect(x, y, blockWidth, blockHeight);
	const int bufferSize = blockWidth * blockHeight * 3;
	std::vector<unsigned char> buffer(bufferSize);
	originScene->readBlock(blockRect, buffer.data(), buffer.size());
	cv::Mat originImage(blockHeight, blockWidth, CV_8UC3, buffer.data());
	std::vector<uint8_t> transformedBuffer(bufferSize);

	ColorTransformation transformation;
	for (auto item : items)
	{
		ColorSpace colorSpace = item.colorSpace;
		int cvTransformation = item.cvTransformation;
		transformation.setColorSpace(colorSpace);
		std::shared_ptr<slideio::Scene> transformedScene = transformScene(originScene, transformation);
		ASSERT_EQ(3, transformedScene->getNumChannels());
		transformedScene->readBlock(blockRect, transformedBuffer.data(), transformedBuffer.size());
		cv::Mat transformedImage(blockHeight, blockWidth, CV_8UC3, transformedBuffer.data());
		cv::Mat originGray;
		cv::cvtColor(originImage, originGray, cvTransformation);
		TestTools::compareRasters(originGray, transformedImage);
	}
}
