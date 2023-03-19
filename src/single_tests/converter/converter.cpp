#include "tests/testlib/testtools.hpp"
#include "slideio/converter/converter.hpp"
#include "slideio/slideio/slideio.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/base/exceptions.hpp"
#include <boost/filesystem.hpp>
#include "slideio/slideio/imagedrivermanager.hpp"


int main()
{
	std::string path = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
	SlidePtr slide = slideio::openSlide(path, "GDAL");
	ScenePtr scene = slide->getScene(0);
	auto sceneRect = scene->getRect();
	int sceneWidth = std::get<2>(sceneRect);
	int sceneHeight = std::get<3>(sceneRect);

	// slideio::TempFile tmp("svs");
	// std::string outputPath = tmp.getPath().string();
	std::string outputPath = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.svs");
	if (boost::filesystem::exists(outputPath)) {
		boost::filesystem::remove(outputPath);
	}
	slideio::ImageTools::JP2KEncodeParameters encodeParameters;
	slideio::ConverterParameters parameters(&encodeParameters);
	parameters.driver = "SVS";
	slideio::convertScene(scene, parameters, outputPath);
	// SlidePtr svsSlide = slideio::openSlide(outputPath, "SVS");
	// ScenePtr svsScene = svsSlide->getScene(0);
	// auto svsRect = svsScene->getRect();
	// int dataSize = sceneHeight * sceneWidth * scene->getNumChannels();
	// std::vector<uint8_t> svsBuffer(dataSize);
	// svsScene->readBlock(sceneRect, svsBuffer.data(), svsBuffer.size());
	// std::vector<uint8_t> gdalBuffer(dataSize);
	// scene->readBlock(sceneRect, gdalBuffer.data(), gdalBuffer.size());
	// cv::Mat svsImage(sceneHeight, sceneWidth, CV_8UC3, svsBuffer.data());
	// cv::Mat gdalImage(sceneHeight, sceneWidth, CV_8UC3, gdalBuffer.data());

    return 0;
   
}
