#include <atomic>
#include <random>
#include <thread>
#include <gtest/gtest.h>
#include "slideio/drivers/ndpi/ndpitifftools.hpp"
#include "tests/testlib/testtools.hpp"
#include <string>
#include <opencv2/imgproc.hpp>
#include "slideio/slideio/imagedrivermanager.hpp"

#include "slideio/drivers/ndpi/ndpiimagedriver.hpp"
#include "slideio/drivers/ndpi/ndpifile.hpp"
#include "slideio/drivers/ndpi/ndpiscene.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/slideio/slideio.hpp"
// Fixture generation only, for colorProfileEndToEndThroughRealDriverPath below.
#include "slideio/imagetools/icctransform.hpp"
#include "slideio/core/tools/tempfile.hpp"
#include "tests/ndpi/synthetic_tiff.hpp"

namespace slideio
{
    class Slide;
}

class NDPIImageDriverTests : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        slideio::ImageDriverManager::setLogLevel("ERROR");
    }
};

TEST_F(NDPIImageDriverTests, openFile)
{
    std::string filePath = TestTools::getTestImagePath("hamamatsu", "2017-02-27 15.29.08.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
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
	EXPECT_EQ(slide->getMetadataFormat(), slideio::MetadataFormat::None);
    EXPECT_EQ(scene->getMetadataFormat(), slideio::MetadataFormat::JSON);
    EXPECT_FALSE(scene->getRawMetadata().empty());
}

TEST_F(NDPIImageDriverTests, readStrippedScene)
{
    std::string filePath = TestTools::getTestImagePath("hamamatsu", "openslide/CMU-1.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    std::string testFilePath1 = TestTools::getTestImagePath("hamamatsu", "openslide/CMU-1-1.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath1);
    std::string testFilePath2 = TestTools::getTestImagePath("hamamatsu", "openslide/CMU-1_002.tif");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath2);
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
    slideio::ImageTools::readSmallImageRaster(testFilePath2, testRaster2);
    cv::resize(testRaster2, testRaster2, blockSize);
    cv::cvtColor(testRaster2, testRaster2, cv::COLOR_BGRA2BGR);
    //TestTools::showRasters(testRaster2, blockRaster);
    double similarity2 = slideio::ImageTools::computeSimilarity2(blockRaster, testRaster2);
    EXPECT_GT(similarity2, 0.95);
}

TEST_F(NDPIImageDriverTests, readROI)
{
    std::string filePath = TestTools::getTestImagePath("hamamatsu", "openslide/CMU-2.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    std::string testFilePath = TestTools::getTestImagePath("hamamatsu", "openslide/CMU-2-roi-l0.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath);
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
    std::string filePath = TestTools::getTestImagePath("hamamatsu", "test3-TRITC 2 (560).ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    std::string testFilePath = TestTools::getTestImagePath("hamamatsu", "test3-TRITC 2 (560)-roi.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath);
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
    std::string filePath = TestTools::getTestImagePath("hamamatsu", "openslide/CMU-2.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    std::string testFilePath = TestTools::getTestImagePath("hamamatsu", "openslide/CMU-2-roi-l0.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath);
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
    std::string filePath = TestTools::getTestImagePath("hamamatsu", "2017-02-27 15.29.08.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    std::string macroFilePath = TestTools::getTestImagePath("hamamatsu", "2017-02-27 15.29.08.macro.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(macroFilePath);
    std::string mapFilePath = TestTools::getTestImagePath("hamamatsu", "2017-02-27 15.29.08.map.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(mapFilePath);
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
    std::string filePath = TestTools::getTestImagePath("hamamatsu", "DM0014 - 2020-04-02 10.25.21.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    std::string testFilePath = TestTools::getTestImagePath("hamamatsu", "DM0014 - 2020-04-02 10.25.21-roi-resampled.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath);
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
    std::string filePath = TestTools::getTestImagePath("hamamatsu", "DM0014 - 2020-04-02 10.25.21.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    std::string testFilePath = TestTools::getTestImagePath("hamamatsu", "DM0014 - 2020-04-02 10.25.21-roi-resampled-tiled.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath);
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
    std::string filePath = TestTools::getTestImagePath("hamamatsu", "DM0014 - 2020-04-02 11.10.47.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    std::string testFilePath = TestTools::getTestImagePath("hamamatsu", "DM0014 - 2020-04-02 11.10.47-resampled.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath);
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
    std::string filePath = TestTools::getTestImagePath("unicode", u8"тест/test3-TRITC 2 (560).ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
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


TEST_F(NDPIImageDriverTests, findZoomDirectory)
{
    const std::string filePath = TestTools::getTestImagePath("hamamatsu", "DM0014 - 2020-04-02 11.10.47.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
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
    const slideio::LevelInfo levels[] = {
        slideio::LevelInfo(0, {11520,9984}, 1.0, 20., {1920,8}),
        slideio::LevelInfo(1, {2880,2496}, 0.25, 5., {480,8}),
        slideio::LevelInfo(2, {720,624},  0.0625, 1.25, {120,8}),
    };
    slideio::NDPIImageDriver driver;
    const std::string filePath = TestTools::getTestImagePath("hamamatsu", "2017-02-27 15.29.08.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
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
    std::string filePath = TestTools::getTestImagePath("hamamatsu", "DM0014 - 2020-04-02 11.10.47.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::NDPIImageDriver driver;
    TestTools::multiThreadedTest(filePath, driver);
}

TEST_F(NDPIImageDriverTests, readRoiExceedScene)
{
    std::string filePath = TestTools::getTestImagePath("hamamatsu", "openslide/CMU-1.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    std::string testFilePath1 = TestTools::getTestImagePath("hamamatsu", "openslide/CMU-1-1.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath1);
    std::string testFilePath2 = TestTools::getTestImagePath("hamamatsu", "openslide/CMU-1_002.tif");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath2);
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
	cv::Rect blockRect = { rect.width - 300, rect.height - 300, 500, 500 };
    cv::Mat blockRaster;
    EXPECT_NO_THROW(scene->readResampledBlock(blockRect, blockRect.size(), blockRaster));
    cv::Size blockSize(100, 100);
    EXPECT_NO_THROW(scene->readResampledBlock(blockRect, blockSize, blockRaster));
}

TEST_F(NDPIImageDriverTests, getDriverId)
{
    std::string filePath = TestTools::getTestImagePath("hamamatsu", "openslide/CMU-1.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    auto slide = slideio::openSlide(filePath, "AUTO");
    ASSERT_TRUE(slide);
    const int numScenes = slide->getNumScenes();
    EXPECT_EQ(1, numScenes);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0)->getCVScene();
    EXPECT_TRUE(scene.get() != nullptr);
	EXPECT_EQ(0, scene->getSceneIndex());
	EXPECT_EQ(filePath, scene->getFilePath());
	EXPECT_EQ("NDPI", scene->getDriverId());
}

// Reading a level whole, by level, has to show the same picture as reading the whole scene
// resampled to that level's size. The second goes through the scene-coordinate round trip
// the level api avoids, so agreement means both address the pyramid the same way.
TEST_F(NDPIImageDriverTests, readLevelMatchesTheResampledSceneRead)
{
    slideio::NDPIImageDriver driver;
    const std::string filePath = TestTools::getTestImagePath("hamamatsu", "2017-02-27 15.29.08.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    auto slide = driver.openFile(filePath);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);
    const int numLevels = scene->getNumZoomLevels();
    ASSERT_EQ(3, numLevels);

    for (int level = 1; level < numLevels; ++level)
    {
        const slideio::LevelInfo* info = scene->getZoomLevelInfo(level);
        ASSERT_TRUE(info != nullptr) << "level " << level;
        const cv::Size levelSize(info->getSize().width, info->getSize().height);
        cv::Mat viaLevel, viaScene;
        scene->readResampledLevelBlockChannels(level, cv::Rect(cv::Point(0, 0), levelSize), levelSize, {}, viaLevel);
        scene->readResampledBlock(scene->getRect(), levelSize, viaScene);
        ASSERT_EQ(levelSize, viaLevel.size()) << "level " << level;
        EXPECT_LE(0.95, slideio::ImageTools::computeSimilarity2(viaLevel, viaScene)) << "level " << level;
    }
}

// The right and bottom edge of a level: the rect overhangs, the read must not throw, and the
// overhang comes back as background. The ndpi SingleStripe branch crops with cv::Mat(m,rect),
// which is exactly the operation that throws when the rect is not contained.
TEST_F(NDPIImageDriverTests, readLevelClampsAnOverhangingRect)
{
    slideio::NDPIImageDriver driver;
    const std::string filePath = TestTools::getTestImagePath("hamamatsu", "2017-02-27 15.29.08.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    auto slide = driver.openFile(filePath);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);
    const int level = scene->getNumZoomLevels() - 1;
    const slideio::LevelInfo* info = scene->getZoomLevelInfo(level);
    ASSERT_TRUE(info != nullptr);
    const cv::Size levelSize(info->getSize().width, info->getSize().height);

    const cv::Rect overhanging(levelSize.width - 64, levelSize.height - 64, 256, 256);
    cv::Mat raster;
    ASSERT_NO_THROW(scene->readResampledLevelBlockChannels(level, overhanging, cv::Size(256, 256), {}, raster));
    ASSERT_EQ(cv::Size(256, 256), raster.size());
    // Anything past the level's own 64x64 corner is background: 255 for a byte image.
    const cv::Vec3b outside = raster.at<cv::Vec3b>(200, 200);
    EXPECT_EQ(255, outside[0]);
    EXPECT_EQ(255, outside[1]);
    EXPECT_EQ(255, outside[2]);
}

// Reading level 0 resampled down to level 1's own size must resample level 0's data, not
// silently escalate to level 1's own directory. Level 1 is its own independently encoded
// image, not an arithmetic derivative of level 0, so an exact pixel match between "level 0
// resampled to level 1's size" and "level 1 read natively" is the base default's reselection
// bug (the old entry point re-derives a zoom from the scene-coordinate rect and picks
// whichever directory best matches it, regardless of the level explicitly requested) — not a
// coincidence of similar content.
TEST_F(NDPIImageDriverTests, readLevelDoesNotEscalateToACoarserLevel)
{
    slideio::NDPIImageDriver driver;
    const std::string filePath = TestTools::getTestImagePath("hamamatsu", "2017-02-27 15.29.08.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    auto slide = driver.openFile(filePath);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);
    ASSERT_EQ(3, scene->getNumZoomLevels());

    const slideio::LevelInfo* coarserInfo = scene->getZoomLevelInfo(1);
    ASSERT_TRUE(coarserInfo != nullptr);
    const cv::Size coarserSize(coarserInfo->getSize().width, coarserInfo->getSize().height);

    cv::Mat resampledFine, coarseNative;
    scene->readResampledLevelBlockChannels(0, scene->getRect(), coarserSize, {}, resampledFine);
    scene->readResampledLevelBlockChannels(1, cv::Rect(cv::Point(0, 0), coarserSize), coarserSize, {}, coarseNative);

    ASSERT_EQ(coarserSize, resampledFine.size());
    ASSERT_EQ(coarserSize, coarseNative.size());
    cv::Mat diff;
    cv::absdiff(resampledFine, coarseNative, diff);
    const double maxAbsDiff = cv::norm(diff, cv::NORM_INF);
    EXPECT_LT(0., maxAbsDiff);
}

TEST_F(NDPIImageDriverTests, concurrentReadsAreByteIdentical) {
    std::string filePath = TestTools::getTestImagePath("hamamatsu", "DM0014 - 2020-04-02 11.10.47.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::NDPIImageDriver driver;
    TestTools::concurrentReadIdentityTest(filePath, driver);
}

TEST_F(NDPIImageDriverTests, reportsConcurrentReadSupport) {
    std::string filePath = TestTools::getTestImagePath("hamamatsu", "DM0014 - 2020-04-02 11.10.47.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::NDPIImageDriver driver;
    auto slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene);
    EXPECT_TRUE(scene->supportsConcurrentReads());
}

// Scenes of one NDPI file share the pool, so opening a second scene must not
// double the descriptor count.
// A sequential read from every scene proves nothing here: a per-scene pool would pass
// identically, since nothing would observe pool identity or descriptor count, and
// DM0014's 3 scenes (main + macro + map aux images) are nowhere near enough to exhaust
// descriptors even duplicated. So this drives concurrency through the main scene ONLY
// -- the aux scene (a DIFFERENT NDPIScene reached through the same NDPIFile) is never
// read at all -- and then compares NDPIFile::contextCount() / NDPIScene::contextCount()
// as read via each scene. Under the real (shared) design this is not a coincidence: both
// calls read the size of the literal same ContextPool, so whatever the main scene's
// traffic grows it to, the aux scene reports identically, having read nothing itself.
// Under a per-scene pool, the aux scene's own pool would never have been constructed at
// all (0 contexts) while the main scene's grew from its own traffic -- so the two would
// differ. Unlike comparing two DIFFERENT thread counts against each other (which this
// test used to do), this does not rely on ContextPool::defaultMax() being large enough
// for two different concurrency levels to actually diverge: it holds even when
// defaultMax() == 1, because "never touched, so never constructed" (0) still differs
// from "touched at least once" (>= 1) regardless of the cap.
TEST_F(NDPIImageDriverTests, scenesOfOneFileShareTheHandlePool) {
    std::string filePath = TestTools::getTestImagePath("hamamatsu", "DM0014 - 2020-04-02 11.10.47.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::NDPIImageDriver driver;
    auto slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);

    auto mainScene = std::dynamic_pointer_cast<slideio::NDPIScene>(slide->getScene(0));
    ASSERT_TRUE(mainScene);
    const auto& auxNames = slide->getAuxImageNames();
    ASSERT_FALSE(auxNames.empty()) << "test needs a second scene of the same file";
    auto auxScene = std::dynamic_pointer_cast<slideio::NDPIScene>(slide->getAuxImage(auxNames.front()));
    ASSERT_TRUE(auxScene);

    // init() itself already borrowed the pool once (to validate the file eagerly, then
    // again inside scanFile()/readDirectoryJpegHeaders), but always one context at a
    // time, so the pool never had to grow past 1 before any scene was read.
    EXPECT_EQ(1, mainScene->contextCount());

    const int mainThreads = 6;
    std::atomic<int> readyCount{0};
    std::atomic<bool> go{false};
    // gtest assertion macros are not safe to call off the main test thread -- a failure
    // recorded from a worker can be corrupted or silently lost. So, following the shape
    // TestTools::concurrentReadIdentityTest already uses, workers record outcomes into
    // atomics only; every assertion below happens on the main thread after join().
    std::atomic<int> emptyCount{0};
    std::atomic<int> exceptionCount{0};
    std::vector<std::thread> threads;
    threads.reserve(mainThreads);
    for (int i = 0; i < mainThreads; ++i) {
        threads.emplace_back([&, mainScene]() {
            ++readyCount;
            while (!go.load()) { std::this_thread::yield(); }
            try {
                const cv::Rect rect = mainScene->getRect();
                const cv::Size size(std::min(64, rect.width), std::min(64, rect.height));
                cv::Mat raster;
                mainScene->readResampledBlockChannels(cv::Rect(rect.x, rect.y, size.width, size.height),
                                                      size, {}, raster);
                if (raster.empty()) {
                    ++emptyCount;
                }
            } catch (const std::exception&) {
                ++exceptionCount;
            }
        });
    }
    // Every thread waits here until all mainThreads have started, so the reads genuinely
    // overlap instead of merely happening to interleave by scheduling luck.
    while (readyCount.load() < mainThreads) { std::this_thread::yield(); }
    go = true;
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_EQ(0, emptyCount.load());
    EXPECT_EQ(0, exceptionCount.load());

    const int mainCount = mainScene->contextCount();
    const int auxCount = auxScene->contextCount();
    EXPECT_GT(mainCount, 0);
    EXPECT_LE(mainCount, slideio::ContextPool::defaultMax());
    EXPECT_EQ(mainCount, auxCount)
        << "the auxiliary image reported a different pool size than the main scene "
           "despite never having been read itself -- the handle pool is not actually "
           "shared per file";
}

// --- color profile --------------------------------------------------------------------
// No NDPI image in the corpus available to this task carries an ICC tag (checked with a
// raw TIFF IFD walker over all nine reachable hamamatsu files: 2017-02-27 15.29.08.ndpi,
// DM0014 - 2020-04-02 10.25.21.ndpi, DM0014 - 2020-04-02 11.10.47.ndpi,
// HE_Hamamatsu.ndpi, CMU-1.ndpi, CMU-2.ndpi, test3-DAPI-2-(387).ndpi, test3-FITC 2
// (485).ndpi, test3-TRITC 2 (560).ndpi -- none carry TIFFTAG_ICCPROFILE). This is the
// absent-path counterpart to colorProfileEndToEndThroughRealDriverPath below.
TEST(NDPIImageDriver, colorProfileMatchesTheTiffTag)
{
    std::string path = TestTools::getTestImagePath("hamamatsu", "openslide/CMU-1.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, "NDPI");
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    const slideio::ColorProfile profile = scene->getColorProfile();
    if (!profile.isEmpty()) {
        ASSERT_EQ(slideio::ColorProfileSource::Embedded, profile.getSource());
        ASSERT_TRUE(scene->getColorProfileInfo().present);
    }
    else {
        ASSERT_EQ(slideio::ColorProfileSource::None, profile.getSource());
    }
}

// The real corpus has no ICC-tagged NDPI file, so this drives the whole real production
// path -- NDPIImageDriver::openFile -> NDPISlide::init -> NDPIFile::init/scanFile ->
// NDPITiffTools::scanTiffDirTags -> NDPISlide::constructScenes -> NDPIScene::init -- on a
// synthetic single-directory TIFF carrying a real embedded sRGB profile, proving the
// wiring end to end rather than only in the absent-tag case every real corpus file
// exercises. No mock of any NDPI class is used; the fixture is a real file and every
// call from openSlide down is the production code.
TEST(NDPIImageDriver, colorProfileEndToEndThroughRealDriverPath)
{
    const slideio::ColorProfile injected = slideio::IccTransform::createSRGBProfile();
    ASSERT_FALSE(injected.isEmpty());
    const std::vector<uint8_t>& profileBytes = injected.getData();

    slideio::TempFile tempTiff("ndpi");
    const std::string tempPath = tempTiff.getPath().string();
    slideio_test::writeSyntheticIccTiff(tempPath, profileBytes);

    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(tempPath, "NDPI");
    ASSERT_TRUE(slide != nullptr);
    ASSERT_EQ(1, slide->getNumScenes());
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);

    const slideio::ColorProfile profile = scene->getColorProfile();
    ASSERT_FALSE(profile.isEmpty());
    EXPECT_EQ(profileBytes, profile.getData());
    EXPECT_EQ(slideio::ColorProfileSource::Embedded, profile.getSource());
    EXPECT_TRUE(scene->getColorProfileInfo().present);
}
