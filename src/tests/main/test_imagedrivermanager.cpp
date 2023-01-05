#include <gtest/gtest.h>
#include "slideio/slideio/imagedrivermanager.hpp"
#include "slideio/slideio/slideio.hpp"
#include "tests/testlib/testtools.hpp"


GTEST_TEST(ImageDriverManager, getDriverIDs)
{
    std::vector<std::string> driverIDs = slideio::ImageDriverManager::getDriverIDs();
    EXPECT_FALSE(driverIDs.empty());
}

GTEST_TEST(ImageDriverManager, getDriversGlobal)
{
    auto drivers = slideio::getDriverIDs();
    EXPECT_FALSE(drivers.empty());
}

TEST(ImageDriverManager, openSlide)
{
    std::string path = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    std::shared_ptr<slideio::CVSlide> slide = slideio::ImageDriverManager::openSlide(path, "GDAL");
    ASSERT_NE(slide, nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    ASSERT_NE(scene, nullptr);
    const cv::Rect sceneRect = scene->getRect();
    const int sceneWidth = sceneRect.width;
    const int sceneHeight = sceneRect.height;
    const int blockWidth = sceneWidth / 2;
    const int blockHeight = sceneHeight / 2;
    const int blockOriginX = sceneWidth / 4;
    const int blockOriginY = sceneHeight / 4;
    cv::Rect blockRect(blockOriginX, blockOriginY, blockWidth, blockHeight);
    const std::tuple<int, int> blockSize(blockWidth, blockHeight);
    const int numChannels = scene->getNumChannels();
    cv::Mat raster;
    scene->readBlock(blockRect, raster);
}


