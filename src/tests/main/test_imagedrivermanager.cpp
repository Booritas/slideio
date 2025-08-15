#include <gtest/gtest.h>

#include "slideio/core/imagedriver.hpp"
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

TEST(ImageDriverManager, findDriver)
{
    typedef std::pair<std::string, std::string> TestEntity;
    std::list<TestEntity> testList = {
        TestEntity("GDAL", TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png")),
        TestEntity("GDAL", TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg")),
        TestEntity("AFI", TestTools::getTestImagePath("afi", "fs.afi", true)),
        TestEntity("CZI", TestTools::getTestImagePath("czi", "pJP31mCherry.czi")),
        TestEntity("DCM", TestTools::getTestImagePath("dcm", "CT-MONO2-12-lomb-an2")),
        TestEntity("DCM", TestTools::getTestImagePath("dcm", "barre.dev/OT-MONO2-8-hip.dcm")),
        TestEntity("NDPI", TestTools::getTestImagePath("ndpi", "test3-DAPI-2-(387).ndpi")),
        TestEntity("SCN", TestTools::getTestImagePath("scn", "Leica-Fluorescence-1.scn")),
        TestEntity("SVS", TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs")),
        TestEntity("ZVI", TestTools::getTestImagePath("svs", "TOMMAlexaFluor647.zvi"))
    };
    for(const auto& entity: testList) {
        auto driverId = std::get<0>(entity);
        auto path = std::get<1>(entity);
        auto driver = slideio::ImageDriverManager::findDriver(path);
        EXPECT_TRUE(driver.get() != nullptr);
        if(driver.get()!=nullptr) {
            EXPECT_EQ(driverId, driver->getID());
        }
    }

}

TEST(ImageDriverManager, getVersion)
{
	std::string version = slideio::ImageDriverManager::getVersion();
	EXPECT_EQ(version, "2.7.2");
}