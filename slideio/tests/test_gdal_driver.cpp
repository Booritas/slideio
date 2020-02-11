#include <gtest/gtest.h>
#include "slideio/drivers/gdal/gdalimagedriver.hpp"
#include "testtools.hpp"
#include <numeric>

TEST(GDALDriver, driverID)
{
    slideio::GDALImageDriver driver;
    EXPECT_EQ(driver.getID(), "GDAL");
}

TEST(GDALDriver, canOpenFile)
{
    slideio::GDALImageDriver driver;
    EXPECT_TRUE(driver.canOpenFile("abc.png"));
    EXPECT_FALSE(driver.canOpenFile("abc.svs"));
}

TEST(GDALDriver, openPngFile_3chnls_8bit)
{
    slideio::GDALImageDriver driver;
    std::string path = TestTools::getTestImagePath("gdal","img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    std::shared_ptr<slideio::Slide> slide = driver.openFile(path);
    ASSERT_TRUE(slide!=nullptr);
    int numbScenes = slide->getNumbScenes();
    ASSERT_TRUE(numbScenes==1);
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    ASSERT_TRUE(scene!=nullptr);
    EXPECT_EQ(slide->getFilePath(),path);
    EXPECT_EQ(scene->getFilePath(),path);
    int channels = scene->getNumChannels();
    EXPECT_EQ(channels, 3);
    cv::Rect sceneRect = scene->getRect();
    EXPECT_EQ(sceneRect.width, 2448);
    EXPECT_EQ(sceneRect.height, 2448);
    EXPECT_EQ(scene->getChannelDataType(0), slideio::DataType::DT_Byte);
    EXPECT_EQ(scene->getChannelDataType(1), slideio::DataType::DT_Byte);
    EXPECT_EQ(scene->getChannelDataType(2), slideio::DataType::DT_Byte);
    slideio::Resolution res = scene->getResolution();
    EXPECT_EQ(res.x, 0.);
    EXPECT_EQ(res.y, 0.);
}

TEST(GDALDriver, openPngFile_1chnl_8bit)
{
    slideio::GDALImageDriver driver;
    std::string path = TestTools::getTestImagePath("gdal","img_2448x2448_1x8bit_SRC_GRAY_ducks.png");
    std::shared_ptr<slideio::Slide> slide = driver.openFile(path);
    ASSERT_TRUE(slide!=nullptr);
    int numbScenes = slide->getNumbScenes();
    ASSERT_TRUE(numbScenes==1);
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    ASSERT_TRUE(scene!=nullptr);
    EXPECT_EQ(slide->getFilePath(),path);
    EXPECT_EQ(scene->getFilePath(),path);
    int channels = scene->getNumChannels();
    EXPECT_EQ(channels, 1);
    cv::Rect sceneRect = scene->getRect();
    EXPECT_EQ(sceneRect.width, 2448);
    EXPECT_EQ(sceneRect.height, 2448);
    EXPECT_EQ(scene->getChannelDataType(0), slideio::DataType::DT_Byte);
    slideio::Resolution res = scene->getResolution();
    EXPECT_EQ(res.x, 0.);
    EXPECT_EQ(res.y, 0.);
}

TEST(GDALDriver, openPngFile_3chnl_16bit)
{
    slideio::GDALImageDriver driver;
    std::string path = TestTools::getTestImagePath("gdal","img_2448x2448_3x16bit_SRC_RGB_ducks.png");
    std::shared_ptr<slideio::Slide> slide = driver.openFile(path);
    ASSERT_TRUE(slide!=nullptr);
    int numbScenes = slide->getNumbScenes();
    ASSERT_TRUE(numbScenes==1);
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    ASSERT_TRUE(scene!=nullptr);
    EXPECT_EQ(slide->getFilePath(),path);
    EXPECT_EQ(scene->getFilePath(),path);
    int channels = scene->getNumChannels();
    EXPECT_EQ(channels, 3);
    cv::Rect sceneRect = scene->getRect();
    EXPECT_EQ(sceneRect.width, 2448);
    EXPECT_EQ(sceneRect.height, 2448);
    EXPECT_EQ(scene->getChannelDataType(0), slideio::DataType::DT_UInt16);
    EXPECT_EQ(scene->getChannelDataType(1), slideio::DataType::DT_UInt16);
    EXPECT_EQ(scene->getChannelDataType(2), slideio::DataType::DT_UInt16);
    slideio::Resolution res = scene->getResolution();
    EXPECT_EQ(res.x, 0.);
    EXPECT_EQ(res.y, 0.);
}

TEST(GDALDriver, openJpegFile_3chnl_8bit)
{
    slideio::GDALImageDriver driver;
    std::string path = TestTools::getTestImagePath("gdal","Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
    std::shared_ptr<slideio::Slide> slide = driver.openFile(path);
    ASSERT_TRUE(slide!=nullptr);
    int numbScenes = slide->getNumbScenes();
    ASSERT_TRUE(numbScenes==1);
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    ASSERT_TRUE(scene!=nullptr);
    EXPECT_EQ(slide->getFilePath(),path);
    EXPECT_EQ(scene->getFilePath(),path);
    int channels = scene->getNumChannels();
    EXPECT_EQ(channels, 3);
    cv::Rect sceneRect = scene->getRect();
    EXPECT_EQ(sceneRect.width, 5494);
    EXPECT_EQ(sceneRect.height, 5839);
    EXPECT_EQ(scene->getChannelDataType(0), slideio::DataType::DT_Byte);
    EXPECT_EQ(scene->getChannelDataType(1), slideio::DataType::DT_Byte);
    EXPECT_EQ(scene->getChannelDataType(2), slideio::DataType::DT_Byte);
    slideio::Resolution res = scene->getResolution();
    EXPECT_EQ(res.x, 0.);
    EXPECT_EQ(res.y, 0.);
}

TEST(GDALDriver, readBlockPng)
{
    slideio::GDALImageDriver driver;
    std::string path = TestTools::getTestImagePath("gdal","img_1024x600_3x8bit_RGB_color_bars_CMYKWRGB.png");
    std::shared_ptr<slideio::Slide> slide = driver.openFile(path);
    ASSERT_TRUE(slide!=nullptr);
    int numbScenes = slide->getNumbScenes();
    ASSERT_TRUE(numbScenes==1);
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    ASSERT_TRUE(scene!=nullptr);
    int channels = scene->getNumChannels();
    EXPECT_EQ(channels, 3);
    cv::Rect sceneRect = scene->getRect();
    EXPECT_EQ(sceneRect.width, 1024);
    EXPECT_EQ(sceneRect.height, 600);
    EXPECT_EQ(scene->getChannelDataType(0), slideio::DataType::DT_Byte);
    cv::Rect block = {260,500,100,100};
    cv::Mat blockRaster;

    scene->readBlock(block, blockRaster);
    cv::Scalar colorMean, colorStddev;
    cv::meanStdDev(blockRaster, colorMean, colorStddev);
    EXPECT_EQ(colorMean[0],255);
    EXPECT_EQ(colorStddev[0],0);

    EXPECT_EQ(colorMean[1], 255);
    EXPECT_EQ(colorStddev[1], 0);

    EXPECT_EQ(colorMean[2], 0);
    EXPECT_EQ(colorStddev[2], 0);

    cv::Mat channelRaster;
    std::vector<int> channelIndices = { 1 };
    scene->readBlockChannels(block, channelIndices, channelRaster);

    cv::Scalar channelMean, channelStddev;
    cv::meanStdDev(channelRaster, channelMean, channelStddev);
    EXPECT_EQ(channelMean[0], 255);
    EXPECT_EQ(channelStddev[0], 0);
}

TEST(GDALDriver, readBlockPngResampling)
{
    slideio::GDALImageDriver driver;
    std::string path = TestTools::getTestImagePath("gdal","img_1024x600_3x8bit_RGB_color_bars_CMYKWRGB.png");
    std::shared_ptr<slideio::Slide> slide = driver.openFile(path);
    ASSERT_TRUE(slide!=nullptr);
    int numbScenes = slide->getNumbScenes();
    ASSERT_TRUE(numbScenes==1);
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    ASSERT_TRUE(scene!=nullptr);
    int channels = scene->getNumChannels();
    EXPECT_EQ(channels, 3);
    cv::Rect rectScene = scene->getRect();
    EXPECT_EQ(rectScene.width, 1024);
    EXPECT_EQ(rectScene.height, 600);
    EXPECT_EQ(scene->getChannelDataType(0), slideio::DataType::DT_Byte);
    cv::Rect blockRect = {260,500, 100,100};
    cv::Size blockSize = {12,12};
    cv::Mat blockRaster;
    scene->readResampledBlock(blockRect, blockSize, blockRaster);
    cv::Scalar colorMean, colorStddev;
    cv::meanStdDev(blockRaster, colorMean, colorStddev);
    EXPECT_EQ(colorMean[0], 255);
    EXPECT_EQ(colorStddev[0], 0);

    EXPECT_EQ(colorMean[1], 255);
    EXPECT_EQ(colorStddev[1], 0);

    EXPECT_EQ(colorMean[2], 0);
    EXPECT_EQ(colorStddev[2], 0);
}
