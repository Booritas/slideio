﻿#include <random>
#include <gtest/gtest.h>
#include "slideio/drivers/ndpi/ndpitifftools.hpp"
#include "tests/testlib/testtools.hpp"
#include <string>
#include <opencv2/imgproc.hpp>
#include "slideio/core/imagedrivermanager.hpp"

#include "slideio/drivers/ndpi/ndpiimagedriver.hpp"
#include "slideio/drivers/ndpi/ndpifile.hpp"
#include "slideio/drivers/ndpi/ndpiscene.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/core/tools/tools.hpp"

namespace slideio
{
    class Slide;
}

class NDPIImageDriverTests : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        slideio::ImageDriverManager::setLogLevel("ERROR");
        std::cerr << "SetUpTestSuite: Running before all tests\n";
    }
    static void TearDownTestSuite() {
    }
};

TEST_F(NDPIImageDriverTests, openFile)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }

    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "2017-02-27 15.29.08.ndpi");
    slideio::NDPIImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);
    const int numScenes = slide->getNumScenes();
    EXPECT_EQ(1, numScenes);
    const std::list<std::string>& names = slide->getAuxImageNames();
    EXPECT_EQ(2, names.size());
    EXPECT_EQ(std::string("macro"), names.front());
    EXPECT_EQ(std::string("map"), names.back());    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    ASSERT_TRUE(scene);
    cv::Rect rect = scene->getRect();
    EXPECT_EQ(rect.x, 0);
    EXPECT_EQ(rect.y, 0);
    EXPECT_EQ(rect.width, 11520);
    EXPECT_EQ(rect.height, 9984);
    int channels = scene->getNumChannels();
    EXPECT_EQ(3, channels);
    slideio::Resolution  res = scene->getResolution();
    EXPECT_DOUBLE_EQ(res.x, 0.45255011992578178e-6);
    EXPECT_DOUBLE_EQ(res.y, 0.45255011992578178e-6);
    double magnification = scene->getMagnification();
    EXPECT_DOUBLE_EQ(20., magnification);
    slideio::Compression compression = scene->getCompression();
    EXPECT_EQ(slideio::Compression::Jpeg, compression);
    slideio::DataType dt = scene->getChannelDataType(0);
    EXPECT_EQ(slideio::DataType::DT_Byte, dt);
}

TEST_F(NDPIImageDriverTests, readStrippedScene)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }

    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-1.ndpi");
    std::string testFilePath1 = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-1-1.png");
    std::string testFilePath2 = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-1_002.tif");
    slideio::NDPIImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);
    const int numScenes = slide->getNumScenes();
    EXPECT_EQ(1, numScenes);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    EXPECT_TRUE(scene.get() != nullptr);
    cv::Rect rect = scene->getRect();
    EXPECT_EQ(rect.x, 0);
    EXPECT_EQ(rect.y, 0);
    EXPECT_EQ(rect.width, 51200);
    EXPECT_EQ(rect.height, 38144);
    int channels = scene->getNumChannels();
    EXPECT_EQ(3, channels);
    slideio::Resolution  res = scene->getResolution();
    EXPECT_DOUBLE_EQ(res.x, 4.5641259698767688e-7);
    EXPECT_DOUBLE_EQ(res.y, 4.5506257110352676e-7);
    double magnification = scene->getMagnification();
    EXPECT_DOUBLE_EQ(20., magnification);
    slideio::Compression compression = scene->getCompression();
    EXPECT_EQ(slideio::Compression::Jpeg, compression);
    slideio::DataType dt = scene->getChannelDataType(0);
    EXPECT_EQ(slideio::DataType::DT_Byte, dt);

    cv::Rect blockRect(rect);
    cv::Size blockSize(400, 298);
    cv::Mat blockRaster;
    scene->readResampledBlock(blockRect, blockSize, blockRaster);
    //TestTools::writePNG(blockRaster, testFilePath1);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath1, testRaster);
    cv::cvtColor(testRaster, testRaster, cv::COLOR_BGRA2BGR);
    double similarity = slideio::ImageTools::computeSimilarity2(blockRaster, testRaster);
    EXPECT_GT(similarity, 0.99);
    //TestTools::showRasters(testRaster, blockRaster);

    blockRect.x = 2000;
    blockRect.y = 20000;
    blockRect.width = 8000;
    blockRect.height = 6000;
    blockSize.width = 800;
    blockSize.height =600;
    scene->readResampledBlock(blockRect, blockSize, blockRaster);
    cv::Mat testRaster2;
    slideio::ImageTools::readGDALImage(testFilePath2, testRaster2);
    cv::resize(testRaster2, testRaster2, blockSize);
    cv::cvtColor(testRaster2, testRaster2, cv::COLOR_BGRA2BGR);
    //TestTools::showRasters(testRaster2, blockRaster);
    double similarity2 = slideio::ImageTools::computeSimilarity2(blockRaster, testRaster2);
    EXPECT_GT(similarity2, 0.95);
}

TEST_F(NDPIImageDriverTests, readROI)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }

    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-2.ndpi");
    std::string testFilePath = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-2-roi-l0.png");
    slideio::NDPIImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);
    const int numScenes = slide->getNumScenes();
    EXPECT_EQ(1, numScenes);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    EXPECT_TRUE(scene.get() != nullptr);
    cv::Rect rect = scene->getRect();
    EXPECT_EQ(rect.x, 0);
    EXPECT_EQ(rect.y, 0);
    EXPECT_EQ(rect.width, 79872);
    EXPECT_EQ(rect.height, 33792);
    int channels = scene->getNumChannels();
    EXPECT_EQ(3, channels);
    slideio::Resolution  res = scene->getResolution();
    EXPECT_NEAR(res.x, 4.564e-7, 0.02e-7);
    EXPECT_NEAR(res.y, 4.564e-7, 0.02e-7);
    double magnification = scene->getMagnification();
    EXPECT_DOUBLE_EQ(20., magnification);
    slideio::Compression compression = scene->getCompression();
    EXPECT_EQ(slideio::Compression::Jpeg, compression);
    slideio::DataType dt = scene->getChannelDataType(0);
    EXPECT_EQ(slideio::DataType::DT_Byte, dt);

    const int blockWidth = 1000;
    const int blockHeight = 800;
    cv::Rect blockRect(15500, 16500, blockWidth, blockHeight);
    cv::Size blockSize(blockWidth, blockHeight);
    cv::Mat blockRaster;
    scene->readResampledBlock(blockRect, blockSize, blockRaster);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    cv::cvtColor(testRaster, testRaster, cv::COLOR_BGRA2BGR);
    double similarity = slideio::ImageTools::computeSimilarity2(blockRaster, testRaster);
    EXPECT_GE(similarity, 0.99);
    //TestTools::showRasters(testRaster, blockRaster);
}

TEST_F(NDPIImageDriverTests, readROI2)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }

    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "test3-TRITC 2 (560).ndpi");
    std::string testFilePath = TestTools::getFullTestImagePath("hamamatsu", "test3-TRITC 2 (560)-roi.png");
    slideio::NDPIImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);
    const int numScenes = slide->getNumScenes();
    EXPECT_EQ(1, numScenes);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    EXPECT_TRUE(scene.get() != nullptr);

    cv::Rect blockRect(1000, 1000, 2000, 1000);
    cv::Size blockSize(500, 250);
    cv::Mat blockRaster;
    scene->readResampledBlock(blockRect, blockSize, blockRaster);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    cv::cvtColor(testRaster, testRaster, cv::COLOR_BGRA2BGR);
    cv::resize(testRaster, testRaster, blockSize);
    double similarity = slideio::ImageTools::computeSimilarity2(blockRaster, testRaster);
    EXPECT_GE(similarity, 0.99);
    //TestTools::showRasters(testRaster, blockRaster);
}


TEST_F(NDPIImageDriverTests, readROIResampled)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }

    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-2.ndpi");
    std::string testFilePath = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-2-roi-l0.png");
    slideio::NDPIImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);
    const int numScenes = slide->getNumScenes();
    EXPECT_EQ(1, numScenes);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    EXPECT_TRUE(scene.get() != nullptr);
    cv::Rect rect = scene->getRect();
    EXPECT_EQ(rect.x, 0);
    EXPECT_EQ(rect.y, 0);
    EXPECT_EQ(rect.width, 79872);
    EXPECT_EQ(rect.height, 33792);
    int channels = scene->getNumChannels();
    EXPECT_EQ(3, channels);
    slideio::Resolution  res = scene->getResolution();
    EXPECT_NEAR(res.x, 4.564e-7, 0.02e-7);
    EXPECT_NEAR(res.y, 4.564e-7, 0.02e-7);
    double magnification = scene->getMagnification();
    EXPECT_DOUBLE_EQ(20., magnification);
    slideio::Compression compression = scene->getCompression();
    EXPECT_EQ(slideio::Compression::Jpeg, compression);
    slideio::DataType dt = scene->getChannelDataType(0);
    EXPECT_EQ(slideio::DataType::DT_Byte, dt);

    const int blockWidth = 1000;
    const int blockHeight = 800;
    const int resizedBlockWidth = 200;
    const int resizedBlockHeight = 160;

    cv::Rect blockRect(15500, 16500, blockWidth, blockHeight);
    cv::Size blockSize(resizedBlockWidth, resizedBlockHeight);
    cv::Mat blockRaster;
    scene->readResampledBlock(blockRect, blockSize, blockRaster);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    cv::resize(testRaster, testRaster, cv::Size(resizedBlockWidth, resizedBlockHeight));
    cv::cvtColor(testRaster, testRaster, cv::COLOR_BGRA2BGR);
    double similarity = slideio::ImageTools::computeSimilarity2(blockRaster, testRaster);
    EXPECT_GE(similarity, 0.94);
    //TestTools::showRasters(testRaster, blockRaster);
}

TEST_F(NDPIImageDriverTests, readAuxImages)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }

    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "2017-02-27 15.29.08.ndpi");
    std::string macroFilePath = TestTools::getFullTestImagePath("hamamatsu", "2017-02-27 15.29.08.macro.png");
    std::string mapFilePath = TestTools::getFullTestImagePath("hamamatsu", "2017-02-27 15.29.08.map.png");
    slideio::NDPIImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);
    const int numAuxImages = slide->getNumAuxImages();
    EXPECT_EQ(2, numAuxImages);
    std::list<std::string> names = slide->getAuxImageNames();
    EXPECT_EQ(names.front(), "macro");
    EXPECT_EQ(names.back(), "map");

    std::shared_ptr<slideio::CVScene> macroScene = slide->getAuxImage("macro");
    EXPECT_TRUE(macroScene.get() != nullptr);
    cv::Rect rect = macroScene->getRect();
    cv::Mat macroRaster;
    macroScene->readBlock(rect, macroRaster);
    //TestTools::writePNG(macroRaster, macroFilePath);
    cv::Mat testRaster;
    TestTools::readPNG(macroFilePath, testRaster);
    TestTools::compareRasters(testRaster, macroRaster);

    std::shared_ptr<slideio::CVScene> mapScene = slide->getAuxImage("map");
    EXPECT_TRUE(mapScene.get() != nullptr);
    cv::Rect rectMap = mapScene->getRect();
    cv::Mat mapRaster;
    mapScene->readBlock(rectMap, mapRaster);
    //TestTools::writePNG(mapRaster, mapFilePath);
    cv::Mat mapTestRaster;
    TestTools::readPNG(mapFilePath, mapTestRaster);
    TestTools::compareRasters(mapRaster, mapTestRaster);
}

TEST_F(NDPIImageDriverTests, readResampledTiled)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }
    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "DM0014 - 2020-04-02 10.25.21.ndpi");
    std::string testFilePath = TestTools::getFullTestImagePath("hamamatsu", "DM0014 - 2020-04-02 10.25.21-roi-resampled.png");
    slideio::NDPIImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);
    const int numScenes = slide->getNumScenes();
    EXPECT_EQ(1, numScenes);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    EXPECT_TRUE(scene.get() != nullptr);
    cv::Rect rect = scene->getRect();
    EXPECT_EQ(rect.x, 0);
    EXPECT_EQ(rect.y, 0);
    EXPECT_EQ(rect.width, 69888);
    EXPECT_EQ(rect.height, 34944);
    int channels = scene->getNumChannels();
    EXPECT_EQ(1, channels);
    slideio::Resolution  res = scene->getResolution();
    EXPECT_DOUBLE_EQ(res.x, 4.4163759219184738e-07);
    EXPECT_DOUBLE_EQ(res.y, 4.4163759219184738e-07);
    double magnification = scene->getMagnification();
    EXPECT_DOUBLE_EQ(20., magnification);
    slideio::Compression compression = scene->getCompression();
    EXPECT_EQ(slideio::Compression::JpegXR, compression);
    slideio::DataType dt = scene->getChannelDataType(0);
    EXPECT_EQ(slideio::DataType::DT_UInt16, dt);

    cv::Mat blockRaster;
    double coefficient = 500. / rect.width;
    cv::Size blockSize(std::lround(rect.width * coefficient), std::lround(rect.height * coefficient));
    scene->readResampledBlock(rect, blockSize, blockRaster);
    //TestTools::showRaster(blockRaster);
    //TestTools::writePNG(blockRaster, testFilePath);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    TestTools::compareRasters(testRaster, blockRaster);
}

TEST_F(NDPIImageDriverTests, readResampledTiledRoi)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }

    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "DM0014 - 2020-04-02 10.25.21.ndpi");
    std::string testFilePath = TestTools::getFullTestImagePath("hamamatsu", "DM0014 - 2020-04-02 10.25.21-roi-resampled-tiled.png");
    slideio::NDPIImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);
    const int numScenes = slide->getNumScenes();
    EXPECT_EQ(1, numScenes);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    EXPECT_TRUE(scene.get() != nullptr);
    cv::Rect rect = scene->getRect();
    EXPECT_EQ(rect.x, 0);
    EXPECT_EQ(rect.y, 0);
    EXPECT_EQ(rect.width, 69888);
    EXPECT_EQ(rect.height, 34944);
    int channels = scene->getNumChannels();
    EXPECT_EQ(1, channels);
    slideio::Resolution  res = scene->getResolution();
    EXPECT_DOUBLE_EQ(res.x, 4.4163759219184738e-07);
    EXPECT_DOUBLE_EQ(res.y, 4.4163759219184738e-07);
    double magnification = scene->getMagnification();
    EXPECT_DOUBLE_EQ(20., magnification);
    slideio::Compression compression = scene->getCompression();
    EXPECT_EQ(slideio::Compression::JpegXR, compression);
    slideio::DataType dt = scene->getChannelDataType(0);
    EXPECT_EQ(slideio::DataType::DT_UInt16, dt);
    cv::Rect blockRect = { rect.width / 2, rect.height / 2, 2000, 2000 };
    cv::Mat blockRaster;
    double coefficient = 500. / blockRect.width;
    cv::Size blockSize(std::lround(blockRect.width * coefficient), std::lround(blockRect.height * coefficient));
    scene->readResampledBlock(blockRect, blockSize, blockRaster);
    //TestTools::showRaster(blockRaster);
    //TestTools::writePNG(blockRaster, testFilePath);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    TestTools::compareRasters(testRaster, blockRaster);
}

TEST_F(NDPIImageDriverTests, readResampled)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }
    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "DM0014 - 2020-04-02 11.10.47.ndpi");
    std::string testFilePath = TestTools::getFullTestImagePath("hamamatsu", "DM0014 - 2020-04-02 11.10.47-resampled.png");
    slideio::NDPIImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);
    const int numScenes = slide->getNumScenes();
    EXPECT_EQ(1, numScenes);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    EXPECT_TRUE(scene.get() != nullptr);
    cv::Rect rect = scene->getRect();
    cv::Mat blockRaster;
    double coefficient = 500. / rect.width;
    cv::Size blockSize(std::lround(rect.width * coefficient), std::lround(rect.height * coefficient));
    scene->readResampledBlock(rect, blockSize, blockRaster);
    //TestTools::showRaster(blockRaster);
    //TestTools::writePNG(blockRaster, testFilePath);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    TestTools::compareRasters(testRaster, blockRaster);
}

TEST_F(NDPIImageDriverTests, openFileUtf8)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }
    {
        std::string filePath = TestTools::getFullTestImagePath("unicode", u8"тест/test3-TRITC 2 (560).ndpi");
        slideio::NDPIImageDriver driver;
        std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
        int dirCount = slide->getNumScenes();
        ASSERT_EQ(dirCount, 1);
        std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
        auto rect = scene->getRect();
        cv::Rect expectedRect(0, 0, 3968, 4864);
        EXPECT_EQ(rect, expectedRect);
        cv::Mat raster;
        scene->readBlock(rect, raster);
        EXPECT_EQ(raster.cols, rect.width);
        EXPECT_EQ(raster.rows, rect.height);
    }
}


TEST_F(NDPIImageDriverTests, findZoomDirectory)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }
    const std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "DM0014 - 2020-04-02 11.10.47.ndpi");
    slideio::NDPIImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    const int dirCount = slide->getNumScenes();
    ASSERT_EQ(dirCount, 1);
    std::shared_ptr<slideio::NDPIScene> scene = std::static_pointer_cast<slideio::NDPIScene>(slide->getScene(0));
    const auto imageRect = scene->getRect();

    // Define the input parameters for the function
    const cv::Rect imageBlockRect(0, 0, 200, 200);

    {
        cv::Size requiredBlockSize(200, 200);
        const slideio::NDPITiffDirectory& dir = scene->findZoomDirectory(imageBlockRect, requiredBlockSize);
        double scale = static_cast<double>(imageRect.width) / static_cast<double>(dir.width);
        EXPECT_DOUBLE_EQ(1., scale);
    }

    {
        cv::Size requiredBlockSize(150, 150);
        const slideio::NDPITiffDirectory& dir = scene->findZoomDirectory(imageBlockRect, requiredBlockSize);
        double scale = static_cast<double>(imageRect.width) / static_cast<double>(dir.width);
        EXPECT_DOUBLE_EQ(1., scale);
    }

    {
        cv::Size requiredBlockSize(100, 100);
        const slideio::NDPITiffDirectory& dir = scene->findZoomDirectory(imageBlockRect, requiredBlockSize);
        double scale = static_cast<double>(imageRect.width) / static_cast<double>(dir.width);
        EXPECT_DOUBLE_EQ(2., scale);
    }

    {
        cv::Size requiredBlockSize(75, 75);
        const slideio::NDPITiffDirectory& dir = scene->findZoomDirectory(imageBlockRect, requiredBlockSize);
        double scale = static_cast<double>(imageRect.width) / static_cast<double>(dir.width);
        EXPECT_DOUBLE_EQ(2., scale);
    }

}

TEST_F(NDPIImageDriverTests, zoomLevels)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }

    const slideio::LevelInfo levels[] = {
        slideio::LevelInfo(0, {11520,9984}, 1.0, 20., {1920,8}),
        slideio::LevelInfo(1, {2880,2496}, 0.25, 5., {480,8}),
        slideio::LevelInfo(2, {720,624},  0.0625, 1.25, {120,8}),
    };
    slideio::NDPIImageDriver driver;
    const std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "2017-02-27 15.29.08.ndpi");
    const std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    const std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    const int numScenes = slide->getNumScenes();
    const cv::Rect rect = scene->getRect();
    double magnification = scene->getMagnification();
    ASSERT_TRUE(scene != nullptr);
    const int numLevels = scene->getNumZoomLevels();
    ASSERT_EQ(3, numLevels);
    for (int levelIndex = 0; levelIndex < numLevels; ++levelIndex)
    {
        const slideio::LevelInfo* level = scene->getZoomLevelInfo(levelIndex);
        EXPECT_EQ(*level, levels[levelIndex]);
        if (levelIndex == 0) {
            EXPECT_EQ(level->getSize(), slideio::Tools::cvSizeToSize(rect.size()));
        }

    }
}

TEST_F(NDPIImageDriverTests, multiThreadSceneAccess) {
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() <<
            "Skip the test because full dataset is not enabled";
    }
    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "DM0014 - 2020-04-02 11.10.47.ndpi");
    slideio::NDPIImageDriver driver;
    TestTools::multiThreadedTest(filePath, driver);
}
