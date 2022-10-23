#include <gtest/gtest.h>
#include "slideio/drivers/ndpi/ndpitifftools.hpp"
#include "tests/testlib/testtools.hpp"
#include <string>
#include <opencv2/highgui.hpp>

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
    std::string testFilePath = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-1.bin");
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
    cv::namedWindow( "Display window", cv::WINDOW_AUTOSIZE );
    cv::imshow( "Display window", blockRaster);
    cv::waitKey(0);
}

