#include <gtest/gtest.h>
#include "slideio/drivers/ndpi/ndpitifftools.hpp"
#include "tests/testlib/testtools.hpp"
#include <string>
#include "slideio/drivers/ndpi/ndpiimagedriver.hpp"


TEST(NDPIImageDriver, openFile)
{
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
    const int imageWidth = 1500;
    double cof = static_cast<double>(imageWidth) / blockRect.width;
    blockSize.width = imageWidth;
    blockSize.height = std::lround(cof * blockRect.height);
    scene->readResampledBlock(blockRect, blockSize, blockRaster);
    //TestTools::writePNG(blockRaster, testFilePath2);
    TestTools::readPNG(testFilePath2, testRaster);
}

TEST(NDPIImageDriver, readROI)
{
    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "B05-41379_10.ndpi");
    std::string testFilePath = TestTools::getFullTestImagePath("hamamatsu", "B05-41379_10-roi.png");
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
    EXPECT_EQ(rect.width, 106496);
    EXPECT_EQ(rect.height, 80896);
    int channels = scene->getNumChannels();
    EXPECT_EQ(3, channels);
    slideio::Resolution  res = scene->getResolution();
    EXPECT_DOUBLE_EQ(res.x, 2.2834699609526636e-7);
    EXPECT_DOUBLE_EQ(res.y, 2.2834699609526636e-7);
    double magnification = scene->getMagnification();
    EXPECT_DOUBLE_EQ(40., magnification);
    slideio::Compression compression = scene->getCompression();
    EXPECT_EQ(slideio::Compression::Jpeg, compression);
    slideio::DataType dt = scene->getChannelDataType(0);
    EXPECT_EQ(slideio::DataType::DT_Byte, dt);

    const int blockWidth = 300;
    const int blockHeight = 300;
    cv::Rect blockRect(rect.width/3, rect.height/3, blockWidth, blockHeight);
    cv::Size blockSize(blockWidth, blockHeight);
    cv::Mat blockRaster;
    scene->readResampledBlock(blockRect, blockSize, blockRaster);
    //slideio::NDPITestTools::writePNG(blockRaster, testFilePath);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    TestTools::compareRasters(testRaster, blockRaster);
}


TEST(NDPIImageDriver, readROIResampled)
{
    std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "B05-41379_10.ndpi");
    std::string testFilePath = TestTools::getFullTestImagePath("hamamatsu", "B05-41379_10-roi-resampled.png");
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
    EXPECT_EQ(rect.width, 106496);
    EXPECT_EQ(rect.height, 80896);
    int channels = scene->getNumChannels();
    EXPECT_EQ(3, channels);
    slideio::Resolution  res = scene->getResolution();
    EXPECT_DOUBLE_EQ(res.x, 2.2834699609526636e-7);
    EXPECT_DOUBLE_EQ(res.y, 2.2834699609526636e-7);
    double magnification = scene->getMagnification();
    EXPECT_DOUBLE_EQ(40., magnification);
    slideio::Compression compression = scene->getCompression();
    EXPECT_EQ(slideio::Compression::Jpeg, compression);
    slideio::DataType dt = scene->getChannelDataType(0);
    EXPECT_EQ(slideio::DataType::DT_Byte, dt);

    const int blockWidth = 1400;
    const int blockHeight = 800;
    const double scale = 0.6;
    const int scaledWidth = std::lround(scale * blockWidth);
    const int scaledHeight = std::lround(scale * blockHeight);
    cv::Rect blockRect(rect.width / 3, rect.height / 3, blockWidth, blockHeight);
    cv::Size blockSize(scaledWidth, scaledHeight);
    cv::Mat blockRaster;
    scene->readResampledBlock(blockRect, blockSize, blockRaster);
    //slideio::NDPITestTools::writePNG(blockRaster, testFilePath);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    TestTools::compareRasters(testRaster, blockRaster);
}

TEST(NDPIImageDriver, readAuxImages)
{
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
