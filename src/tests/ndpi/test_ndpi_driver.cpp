#include <gtest/gtest.h>
#include "slideio/drivers/ndpi/ndpitifftools.hpp"
#include "tests/testlib/testtools.hpp"
#include <string>
#include <opencv2/imgproc.hpp>

#include "slideio/drivers/ndpi/ndpiimagedriver.hpp"
#include "slideio/drivers/ndpi/ndpifile.hpp"
#include "slideio/drivers/ndpi/ndpiscene.hpp"
#include "slideio/imagetools/imagetools.hpp"

namespace slideio
{
    class Slide;
}

TEST(NDPIImageDriver, openFile)
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

TEST(NDPIImageDriver, readStrippedScene)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }

    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-1.ndpi");
    std::string testFilePath1 = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-1-1.png");
    std::string testFilePath2 = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-1-2.png");
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
    cv::Size blockSize(rect.width / 100, rect.height / 100);
    cv::Mat blockRaster;
    scene->readResampledBlock(blockRect, blockSize, blockRaster);
    //TestTools::writePNG(blockRaster, testFilePath1);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath1, testRaster);
    TestTools::compareRasters(blockRaster, testRaster);
    //TestTools::showRaster(blockRaster);

    blockRect.x = rect.width / 4;
    blockRect.y = rect.height / 4;
    blockRect.width = rect.width / 2;
    blockRect.height = rect.height / 2;
    const int blockNewWidth = 1500;
    double cof = static_cast<double>(blockNewWidth) / blockRect.width;
    blockSize.width = blockNewWidth;
    blockSize.height = std::lround(cof * blockRect.height);
    scene->readResampledBlock(blockRect, blockSize, blockRaster);
    //TestTools::writePNG(blockRaster, testFilePath2);
    TestTools::readPNG(testFilePath2, testRaster);
    TestTools::compareRasters(blockRaster, testRaster);
    //TestTools::showRaster(blockRaster);
}

TEST(NDPIImageDriver, readROI)
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
}

TEST(NDPIImageDriver, readROI2)
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
    //TestTools::showRaster(blockRaster);
}


TEST(NDPIImageDriver, readROIResampled)
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
    cv::cvtColor(testRaster, testRaster, cv::COLOR_BGRA2BGR);
    cv::resize(testRaster, testRaster, cv::Size(resizedBlockWidth, resizedBlockHeight));
    double similarity = slideio::ImageTools::computeSimilarity2(blockRaster, testRaster);
    EXPECT_GE(similarity, 0.95);
}

TEST(NDPIImageDriver, readAuxImages)
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

TEST(NDPIImageDriver, readResampledTiled)
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

TEST(NDPIImageDriver, readResampledTiledRoi)
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

TEST(NDPIImageDriver, readResampled)
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

TEST(NDPIImageDriver, openFileUtf8)
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


TEST(NDPIImageDriver, findZoomDirectory)
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
