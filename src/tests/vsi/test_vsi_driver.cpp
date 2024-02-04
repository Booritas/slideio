#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include <string>
#include <opencv2/imgproc.hpp>

#include "slideio/drivers/vsi/vsiimagedriver.hpp"
#include "slideio/drivers/vsi/vsiscene.hpp"
#include "slideio/drivers/vsi/vsislide.hpp"
#include "slideio/drivers/vsi/vsifile.hpp"


namespace slideio
{
    class Slide;
}

using namespace slideio;

TEST(VSIImageDriver, openFileWithoutExternalFiles)
{
    std::string filePath = TestTools::getFullTestImagePath("vsi",
        "Zenodo/Q6VM49JF/Figure-1-ultrasound-raw-data"
        "/SPECTRUM_#201_2016-06-14_Jiangtao Liu/1286FL9057GDF8RGDX257R2GLHZ.vsi");
    slideio::VSIImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide!=nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(1, numScenes);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    //EXPECT_EQ(scene->getName(), "001 C405, C488");
    auto rect = scene->getRect();
    EXPECT_EQ(rect.width, 608);
    EXPECT_EQ(rect.height, 600);
    EXPECT_EQ(rect.x, 0);
    EXPECT_EQ(rect.y, 0);
    EXPECT_EQ(scene->getNumChannels(), 3);
    for(int channel=0; channel<scene->getNumChannels(); ++channel) {
        EXPECT_EQ(scene->getChannelDataType(channel), DataType::DT_Byte);
    }
    EXPECT_DOUBLE_EQ(scene->getMagnification(), 0.);
    EXPECT_EQ(scene->getCompression(), Compression::Uncompressed);
}

TEST(VSIImageDriver, openFileWithExternalFiles)
{
    std::tuple<std::string,int,int, double, std::string> result[] = {
        {"Overview",15751,7567,2, "Sample Mask"},
        {"40x_01", 14749,20874,40,"40x FocusMap"},
        {"40x_02", 15596,19403,40,"40x FocusMap"},
        {"40x_03", 16240,18759,40,"40x FocusMap"},
    };
    const std::string filePath = TestTools::getFullTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.vsi");
    slideio::VSIImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(4, numScenes);
    for(int sceneIndex=0; sceneIndex<numScenes; ++sceneIndex) {
        std::shared_ptr<CVScene> scene = slide->getScene(sceneIndex);
        EXPECT_EQ(scene->getName(), std::get<0>(result[sceneIndex]));
        auto rect = scene->getRect();
        EXPECT_EQ(rect.width, std::get<1>(result[sceneIndex]));
        EXPECT_EQ(rect.height, std::get<2>(result[sceneIndex]));
        EXPECT_EQ(rect.x, 0);
        EXPECT_EQ(rect.y, 0);
        EXPECT_DOUBLE_EQ(scene->getMagnification(), std::get<3>(result[sceneIndex]));
        EXPECT_EQ(1, scene->getNumAuxImages());
        auto auxImageNames = scene->getAuxImageNames();
        EXPECT_EQ(1, auxImageNames.size());
        EXPECT_EQ(auxImageNames.front(), std::get<4>(result[sceneIndex]));
    }
}

TEST(VSIImageDriver, auxImages)
{
    const std::string filePath = TestTools::getFullTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.vsi");
    const std::string testFilePath = TestTools::getFullTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.aux.png");
    slideio::VSIImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(4, numScenes);
    const int sceneIndex = 0;
    std::shared_ptr<CVScene> scene = slide->getScene(sceneIndex);
    EXPECT_EQ(1, scene->getNumAuxImages());
    auto auxImageNames = scene->getAuxImageNames();
    EXPECT_EQ(1, auxImageNames.size());
    auto auxImage = scene->getAuxImage(auxImageNames.front());
    EXPECT_TRUE(auxImage != nullptr);
    cv::Rect rect = auxImage->getRect();
    cv::Mat raster;
    auxImage->readBlock(rect, raster);
    //TestTools::writePNG(raster, testFilePath);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    TestTools::compareRasters(testRaster, raster);
    //TestTools::showRasters(testRaster, raster);
}


TEST(VSIImageDriver, VSIFileOpenWithOutExternalFiles)
{
    std::string filePath = TestTools::getFullTestImagePath("vsi", 
        "Zenodo/Q6VM49JF/Figure-1-ultrasound-raw-data"
                "/SPECTRUM_#201_2016-06-14_Jiangtao Liu/1286FL9057GDF8RGDX257R2GLHZ.vsi");
    vsi::VSIFile vsiFile(filePath);
    EXPECT_EQ(0, vsiFile.getNumEtsFiles());
}

TEST(VSIImageDriver, readVSISceneStripedDirUncompressed)
{
    std::string filePath = TestTools::getFullTestImagePath("vsi",
        "Zenodo/Q6VM49JF/Figure-1-ultrasound-raw-data"
        "/SPECTRUM_#201_2016-06-14_Jiangtao Liu/1286FL9057GDF8RGDX257R2GLHZ.vsi");
    std::string testFilePath = TestTools::getFullTestImagePath("vsi",
        "test-output/1286FL9057GDF8RGDX257R2GLHZ.png");
    slideio::VSIImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(1, numScenes);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    const auto rect = scene->getRect();
    cv::Mat blockRaster;
    scene->readBlock(rect, blockRaster);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    TestTools::compareRasters(testRaster, blockRaster);
    //TestTools::showRasters(testRaster, blockRaster);
}

TEST(VSIImageDriver, readVSISceneStripedDirUncompressedRoi)
{
    std::string filePath = TestTools::getFullTestImagePath("vsi",
        "Zenodo/Q6VM49JF/Figure-1-ultrasound-raw-data"
        "/SPECTRUM_#201_2016-06-14_Jiangtao Liu/1286FL9057GDF8RGDX257R2GLHZ.vsi");
    std::string testFilePath = TestTools::getFullTestImagePath("vsi",
        "test-output/1286FL9057GDF8RGDX257R2GLHZ.png");
    slideio::VSIImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(1, numScenes);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    const auto rect = scene->getRect();
    cv::Rect roi(rect.x + rect.width/4, rect.y + rect.height/4, rect.width/2, rect.height/2);
    cv::Mat blockRaster;
    scene->readBlock(roi, blockRaster);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    cv::Mat testRoi(testRaster, roi);
    TestTools::compareRasters(testRoi, blockRaster);
    //TestTools::showRasters(testRoi, blockRaster);
}

TEST(VSIImageDriver, readVSISceneStripedDirUncompressedRoiResampled)
{
    std::string filePath = TestTools::getFullTestImagePath("vsi",
        "Zenodo/Q6VM49JF/Figure-1-ultrasound-raw-data"
        "/SPECTRUM_#201_2016-06-14_Jiangtao Liu/1286FL9057GDF8RGDX257R2GLHZ.vsi");
    std::string testFilePath = TestTools::getFullTestImagePath("vsi",
        "test-output/1286FL9057GDF8RGDX257R2GLHZ.png");
    slideio::VSIImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(1, numScenes);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    const auto rect = scene->getRect();
    cv::Rect roi(rect.x + rect.width / 4, rect.y + rect.height / 4, rect.width / 2, rect.height / 2);
    cv::Size blockSize(std::lround(roi.width*0.8), std::lround(roi.height*0.8));
    cv::Mat blockRaster;
    scene->readResampledBlock(roi, blockSize, blockRaster);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    cv::Mat testRoi(testRaster, roi);
    cv::resize(testRoi, testRoi, blockSize);
    TestTools::compareRasters(testRoi, blockRaster);
    //TestTools::showRasters(testRoi, blockRaster);
}

TEST(VSIImageDriver, VSIFileOpenWithExternalFiles)
{
    std::string filePath = TestTools::getFullTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.vsi");
    vsi::VSIFile vsiFile(filePath);
    EXPECT_EQ(4, vsiFile.getNumEtsFiles());
}


TEST(VSIImageDriver, read3DVolume16bit)
{
    std::string filePath = TestTools::getFullTestImagePath("vsi","vsi-multifile/vsi-ets-test-jpg2k.vsi");
    std::string testFilePath = TestTools::getFullTestImagePath("vsi","test-output/vsi-ets-test-jpg2k.tif");
    slideio::VSIImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(1, numScenes);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    const auto rect = scene->getRect();
    EXPECT_EQ(cv::Rect(0, 0, 1645, 1682), rect);
    EXPECT_EQ(2, scene->getNumChannels());
    EXPECT_EQ(DataType::DT_UInt16, scene->getChannelDataType(0));
    EXPECT_EQ(DataType::DT_UInt16, scene->getChannelDataType(1));
    EXPECT_EQ(11, scene->getNumZSlices());
    EXPECT_EQ(1, scene->getNumTFrames());
    const auto resolution = scene->getResolution();
    const double val = 0.108333e-6;
    EXPECT_DOUBLE_EQ(val, resolution.x);
    EXPECT_DOUBLE_EQ(val, resolution.y);
    //cv::Rect roi(rect.x + rect.width / 4, rect.y + rect.height / 4, rect.width / 2, rect.height / 2);
    //cv::Size blockSize(std::lround(roi.width * 0.8), std::lround(roi.height * 0.8));
    //cv::Mat blockRaster;
    //scene->readResampledBlock(roi, blockSize, blockRaster);
    //cv::Mat testRaster;
    //TestTools::readPNG(testFilePath, testRaster);
    //cv::Mat testRoi(testRaster, roi);
    //cv::resize(testRoi, testRoi, blockSize);
    //TestTools::compareRasters(testRoi, blockRaster);
    //TestTools::showRasters(testRoi, blockRaster);
}

TEST(VSIImageDriver, readMultiscene)
{
    std::string filePath = TestTools::getFullTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.vsi");
    std::string testFilePath = TestTools::getFullTestImagePath("vsi", "test-output/G1M16_ABD_HE_B6.tiff");
    slideio::VSIImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(3, numScenes);
    //std::shared_ptr<CVScene> scene = slide->getScene(0);
    //const auto rect = scene->getRect();
    //EXPECT_EQ(cv::Rect(0, 0, 1645, 1682), rect);
    //EXPECT_EQ(2, scene->getNumChannels());
    //cv::Rect roi(rect.x + rect.width / 4, rect.y + rect.height / 4, rect.width / 2, rect.height / 2);
    //cv::Size blockSize(std::lround(roi.width * 0.8), std::lround(roi.height * 0.8));
    //cv::Mat blockRaster;
    //scene->readResampledBlock(roi, blockSize, blockRaster);
    //cv::Mat testRaster;
    //TestTools::readPNG(testFilePath, testRaster);
    //cv::Mat testRoi(testRaster, roi);
    //cv::resize(testRoi, testRoi, blockSize);
    //TestTools::compareRasters(testRoi, blockRaster);
    //TestTools::showRasters(testRoi, blockRaster);
}

