#include "tests/testlib/testtools.hpp"
#include "slideio/slideio/slideio.hpp"
#include <gtest/gtest.h>
#include <boost/filesystem.hpp>
#include <opencv2/imgproc.hpp>
#include <numeric>



TEST(GenericAPI, openSlide)
{
    std::string path = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, "GDAL");
    ASSERT_NE(slide, nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    ASSERT_NE(scene, nullptr);
    const std::tuple<int, int, int, int> sceneRect = scene->getRect();
    const int sceneWidth = std::get<2>(sceneRect);
    const int sceneHeight = std::get<3>(sceneRect);
    const int blockWidth = sceneWidth / 2;
    const int blockHeight = sceneHeight / 2;
    const int blockOriginX = sceneWidth / 4;
    const int blockOriginY = sceneHeight / 4;
    std::tuple<int, int, int, int> blockRect(blockOriginX, blockOriginY, blockWidth, blockHeight);
    const std::tuple<int, int> blockSize(blockWidth, blockHeight);
    const int numChannels = scene->getNumChannels();
    const int memSize = scene->getBlockSize(blockSize, 0, numChannels, 1, 1);
    std::vector<uint8_t> rasterData(memSize);
    scene->readBlock(blockRect, rasterData.data(), rasterData.size());
}
