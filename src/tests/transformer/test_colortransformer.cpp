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

TEST(ColorTransfomation, colors)
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

