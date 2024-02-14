#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include <string>
#include <opencv2/imgproc.hpp>

#include "slideio/core/tools/tools.hpp"
#include "slideio/drivers/vsi/vsiimagedriver.hpp"
#include "slideio/drivers/vsi/vsiscene.hpp"
#include "slideio/drivers/vsi/vsislide.hpp"
#include "slideio/drivers/vsi/vsifile.hpp"
#include "slideio/imagetools/cvtools.hpp"


namespace slideio
{
    class Slide;
}

using namespace slideio;

class TestDimensionOrder : public vsi::IDimensionOrder
{
public:
    TestDimensionOrder(int channelIndex = 2, int zIndex = 3, int tIndex = 4) :
        m_channelIndex(channelIndex), m_zIndex(zIndex), m_tIndex(tIndex) {
    }

    int getDimensionOrder(vsi::Dimensions dim) const override {
        switch (dim) {
        case vsi::Dimensions::X:
            return 0;
        case vsi::Dimensions::Y:
            return 1;
        case vsi::Dimensions::C:
            return m_channelIndex;
        case vsi::Dimensions::Z:
            return m_zIndex;
        case vsi::Dimensions::T:
            return m_tIndex;
        }
        return -1;
    }

private:
    int m_channelIndex;
    int m_zIndex;
    int m_tIndex;
};

TEST(VSIImageDriver, openFileWithoutExternalFiles) {
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
    for (int channel = 0; channel < scene->getNumChannels(); ++channel) {
        EXPECT_EQ(scene->getChannelDataType(channel), DataType::DT_Byte);
    }
    EXPECT_DOUBLE_EQ(scene->getMagnification(), 0.);
    EXPECT_EQ(scene->getCompression(), Compression::Uncompressed);
}

TEST(VSIImageDriver, openFileWithExternalFiles) {
    std::tuple<std::string, int, int, double, std::string> result[] = {
        {"40x_01", 14749, 20874, 40, "40x FocusMap"},
        {"40x_02", 15596, 19403, 40, "40x FocusMap"},
        {"40x_03", 16240, 18759, 40, "40x FocusMap"},
    };
    const std::string filePath = TestTools::getFullTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.vsi");
    slideio::VSIImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(3, numScenes);
    for (int sceneIndex = 0; sceneIndex < numScenes; ++sceneIndex) {
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
    ASSERT_EQ(1, slide->getNumAuxImages());
    auto names = slide->getAuxImageNames();
    ASSERT_EQ(1, names.size());
    EXPECT_EQ("Overview", names.front());
}

TEST(VSIImageDriver, auxImages) {
    const std::string filePath = TestTools::getFullTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.vsi");
    const std::string testFilePath = TestTools::getFullTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.aux.png");
    slideio::VSIImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(3, numScenes);
    EXPECT_EQ(1, slide->getNumAuxImages());
    auto auxImageNames = slide->getAuxImageNames();
    EXPECT_EQ(1, auxImageNames.size());
    //auto auxImage = slide->getAuxImage(auxImageNames.front());
    //EXPECT_TRUE(auxImage != nullptr);
    //cv::Rect rect = auxImage->getRect();
    //cv::Mat raster;
    //auxImage->readBlock(rect, raster);
    ////TestTools::writePNG(raster, testFilePath);
    //cv::Mat testRaster;
    //TestTools::readPNG(testFilePath, testRaster);
    //TestTools::compareRasters(testRaster, raster);
    ////TestTools::showRasters(testRaster, raster);
}


TEST(VSIImageDriver, VSIFileOpenWithOutExternalFiles) {
    std::string filePath = TestTools::getFullTestImagePath("vsi",
                                                           "Zenodo/Q6VM49JF/Figure-1-ultrasound-raw-data"
                                                           "/SPECTRUM_#201_2016-06-14_Jiangtao Liu/1286FL9057GDF8RGDX257R2GLHZ.vsi");
    vsi::VSIFile vsiFile(filePath);
    EXPECT_EQ(0, vsiFile.getNumEtsFiles());
}

TEST(VSIImageDriver, readVSISceneStripedDirUncompressed) {
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
    EXPECT_EQ(slideio::Compression::Uncompressed, scene->getCompression());
    const auto rect = scene->getRect();
    cv::Mat blockRaster;
    scene->readBlock(rect, blockRaster);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    TestTools::compareRasters(testRaster, blockRaster);
    //TestTools::showRasters(testRaster, blockRaster);
}

TEST(VSIImageDriver, readVSISceneStripedDirUncompressedRoi) {
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
    cv::Mat blockRaster;
    scene->readBlock(roi, blockRaster);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    cv::Mat testRoi(testRaster, roi);
    TestTools::compareRasters(testRoi, blockRaster);
    //TestTools::showRasters(testRoi, blockRaster);
}

TEST(VSIImageDriver, readVSISceneStripedDirUncompressedRoiResampled) {
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
    cv::Size blockSize(std::lround(roi.width * 0.8), std::lround(roi.height * 0.8));
    cv::Mat blockRaster;
    scene->readResampledBlock(roi, blockSize, blockRaster);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    cv::Mat testRoi(testRaster, roi);
    cv::resize(testRoi, testRoi, blockSize);
    TestTools::compareRasters(testRoi, blockRaster);
    //TestTools::showRasters(testRoi, blockRaster);
}

TEST(VSIImageDriver, VSIFileOpenWithExternalFiles) {
    std::string filePath = TestTools::getFullTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.vsi");
    vsi::VSIFile vsiFile(filePath);
    EXPECT_EQ(4, vsiFile.getNumEtsFiles());
}

TEST(VSIImageDriver, read3DVolume16bitSlice) {
    std::string filePath = TestTools::getFullTestImagePath("vsi", "vsi-multifile/vsi-ets-test-jpg2k.vsi");
    std::string testFilePath = TestTools::getFullTestImagePath("vsi", "test-output/vsi-ets-test-jpg2k.vsi - 001 C405, C488 (1, x=0, y=0, w=1645, h=1682).tif");
    slideio::VSIImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(1, numScenes);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    const auto rect = scene->getRect();
    cv::Rect roi(rect);
    cv::Size blockSize(roi.width, roi.height);
    cv::Mat blockRaster;
    scene->readResampled4DBlockChannels(roi, blockSize, { 1 }, { 5,6 }, { 0,1 }, blockRaster);
    //cv::Mat testRaster;
    //ImageTools::readGDALImage(testFilePath, testRaster);
    //cv::resize(testRaster, testRaster, blockSize);
    //double similarity = ImageTools::computeSimilarity2(testRaster, blockRaster);
    //EXPECT_GT(similarity, 0.99);
    //TestTools::showRasters(testRaster, blockRaster);
}


TEST(VSIImageDriver, read3DVolume16bit) {
    std::string filePath = TestTools::getFullTestImagePath("vsi", "vsi-multifile/vsi-ets-test-jpg2k.vsi");
    std::string testFilePath = TestTools::getFullTestImagePath("vsi", "test-output/vsi-ets-test-jpg2k.vsi - 001 C405, C488 (1, x=0, y=0, w=1645, h=1682).tif");
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
    EXPECT_DOUBLE_EQ(60, scene->getMagnification());
    EXPECT_DOUBLE_EQ(1.E-6, scene->getZSliceResolution());
    EXPECT_EQ(slideio::Compression::Jpeg2000, scene->getCompression());
    EXPECT_EQ("C405", scene->getChannelName(0));
    EXPECT_EQ("C488", scene->getChannelName(1));
    cv::Rect roi(rect);
    cv::Size blockSize(roi.width/4, roi.height/4);
    cv::Mat blockRaster;
    scene->readResampled4DBlockChannels(roi, blockSize, { 0 }, { 5,6 }, { 0,1 }, blockRaster);
    cv::Mat testRaster;
    ImageTools::readGDALImage(testFilePath, testRaster);
    cv::resize(testRaster, testRaster, blockSize);
    double similarity = ImageTools::computeSimilarity2(testRaster, blockRaster);
    EXPECT_GT(similarity, 0.99);
    //TestTools::showRasters(testRaster, blockRaster);
}

TEST(VSIImageDriver, read3DStack16bit) {
    std::string filePath = TestTools::getFullTestImagePath("vsi", "vsi-multifile/vsi-ets-test-jpg2k.vsi");
    std::string testFilePath5 = TestTools::getFullTestImagePath("vsi", 
        "test-output/vsi-ets-test-jpg2k.vsi - 001 C405, C488 (1, x=0, y=0, w=1645, h=1682).tif");
    std::string testFilePath4 = TestTools::getFullTestImagePath("vsi",
        "test-output/vsi-ets-test-jpg2k.vsi - slice4.(1, x=0, y=0, w=1645, h=1682).tif");
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
    EXPECT_DOUBLE_EQ(60, scene->getMagnification());
    EXPECT_DOUBLE_EQ(1.E-6, scene->getZSliceResolution());
    EXPECT_EQ(slideio::Compression::Jpeg2000, scene->getCompression());
    cv::Rect roi(rect);
    cv::Size blockSize(roi.width, roi.height);
    cv::Mat blockRaster;
    scene->readResampled4DBlockChannels(roi, blockSize, { 0 }, 
        { 4,6 }, { 0,1 }, blockRaster);
    EXPECT_EQ(2,blockRaster.size[2]);
    cv::Mat slice;
    CVTools::extractSliceFrom3D(blockRaster, 0, slice);
    cv::Mat testRaster;
    ImageTools::readGDALImage(testFilePath4, testRaster);
    double similarity = ImageTools::computeSimilarity2(testRaster, slice);
    EXPECT_GT(similarity, 0.99);
    CVTools::extractSliceFrom3D(blockRaster, 1, slice);
    ImageTools::readGDALImage(testFilePath5, testRaster);
    EXPECT_GT(similarity, 0.99);
}

TEST(VSIImageDriver, readMultiscene) {
    std::string filePath = TestTools::getFullTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.vsi");
    std::string testFilePath = TestTools::getFullTestImagePath("vsi", "test-output/G1M16_ABD_HE_B6.vsi-40x_01(1,x=5836,y=11793,w=849,h=607).png");
    slideio::VSIImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(3, numScenes);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    const auto rect = scene->getRect();
    EXPECT_EQ(cv::Rect(0, 0, 14749, 20874), rect);
    EXPECT_EQ(3, scene->getNumChannels());
    EXPECT_EQ(DataType::DT_Byte, scene->getChannelDataType(0));
    Resolution resolution = scene->getResolution();
    EXPECT_DOUBLE_EQ(1.72224E-7, resolution.x);
    EXPECT_DOUBLE_EQ(1.72223E-7, resolution.y);
    EXPECT_DOUBLE_EQ(40., scene->getMagnification());
    EXPECT_EQ(1, scene->getNumZSlices());
    EXPECT_EQ(1, scene->getNumTFrames());
    const int numAuxImages = scene->getNumAuxImages();
    EXPECT_EQ(1, numAuxImages);
    auto auxImageNames = scene->getAuxImageNames();
    EXPECT_EQ(1, auxImageNames.size());
    EXPECT_EQ("40x FocusMap", auxImageNames.front());
    cv::Rect roi(5836,11793,849,607);
    cv::Size blockSize(roi.size());
    cv::Mat blockRaster;
    scene->readResampledBlock(roi, blockSize, blockRaster);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    TestTools::compareRasters(testRaster, blockRaster);
    //TestTools::showRasters(testRaster, blockRaster);
}

TEST(VSIImageDriver, readMultisceneResized) {
    std::string filePath = TestTools::getFullTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.vsi");
    std::string testFilePath = TestTools::getFullTestImagePath("vsi", "test-output/G1M16_ABD_HE_B6.vsi-40x_01(1,x=5836,y=11793,w=849,h=607).png");
    slideio::VSIImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(3, numScenes);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    const auto rect = scene->getRect();
    cv::Rect roi(5836, 11793, 849, 607);
    cv::Size blockSize(roi.width/3, roi.height/3);
    cv::Mat blockRaster;
    scene->readResampledBlock(roi, blockSize, blockRaster);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    cv::resize(testRaster, testRaster, blockSize);
    double similarity = ImageTools::computeSimilarity2(testRaster, blockRaster);
    ASSERT_GT(similarity, 0.99);
    //TestTools::showRasters(testRaster, blockRaster);
}

TEST(VSIImageDriver, readMultisceneResizedSingeChannel) {
    std::string filePath = TestTools::getFullTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.vsi");
    std::string testFilePath = TestTools::getFullTestImagePath("vsi", "test-output/G1M16_ABD_HE_B6.vsi-40x_01(1,x=5836,y=11793,w=849,h=607).png");
    slideio::VSIImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(3, numScenes);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    const auto rect = scene->getRect();
    cv::Rect roi(5836, 11793, 849, 607);
    cv::Size blockSize(roi.width / 3, roi.height / 3);
    cv::Mat blockRaster;
    scene->readResampledBlockChannels(roi, blockSize, {0}, blockRaster);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    cv::resize(testRaster, testRaster, blockSize);
    cv::extractChannel(testRaster,testRaster,0);
    double similarity = ImageTools::computeSimilarity2(testRaster, blockRaster);
    ASSERT_GT(similarity, 0.99);
    //TestTools::showRasters(testRaster, blockRaster);
}

TEST(VSIImageDriver, readMultisceneResizedReversedChannels) {
    std::string filePath = TestTools::getFullTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.vsi");
    std::string testFilePath = TestTools::getFullTestImagePath("vsi", 
        "test-output/G1M16_ABD_HE_B6.vsi-40x_01(1,x=5836,y=11793,w=849,h=607).png");
    slideio::VSIImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(3, numScenes);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    const auto rect = scene->getRect();
    cv::Rect roi(5836, 11793, 849, 607);
    cv::Size blockSize(roi.width / 3, roi.height / 3);
    cv::Mat blockRaster;
    scene->readResampledBlockChannels(roi, blockSize, { 2,1,0 }, blockRaster);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    cv::resize(testRaster, testRaster, blockSize);
    std::vector<cv::Mat> channels(3);
    cv::extractChannel(testRaster, channels[2], 0);
    cv::extractChannel(testRaster, channels[1], 1);
    cv::extractChannel(testRaster, channels[0], 2);
    cv::merge(channels, testRaster);
    double similarity = ImageTools::computeSimilarity2(testRaster, blockRaster);
    ASSERT_GT(similarity, 0.99);
    //TestTools::showRasters(testRaster, blockRaster);
}

TEST(EtsFile, readTileJpeg) {
    std::string filePath = TestTools::getFullTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.vsi");
    std::string testFilePath = TestTools::getFullTestImagePath("vsi", "test-output/G1M16_ABD_HE_B6.vsi-40x_01(1,x=0,y=0,w=512,h=512).png");
    slideio::vsi::VSIFile vsiFile(filePath);
    auto etsFile = vsiFile.getEtsFile(1);
    cv::Mat tileRaster;
    etsFile->readTile(0, 0, {},0, 0,  tileRaster);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    TestTools::compareRasters(testRaster, tileRaster);
    //TestTools::showRasters(testRaster, tileRaster);
}

TEST(EtsFile, readTileJpeg2K) {
    std::string filePath = TestTools::getFullTestImagePath("vsi", "vsi-multifile/vsi-ets-test-jpg2k.vsi");
    std::string testFilePath = TestTools::getFullTestImagePath("vsi", "test-output/vsi-ets-test-jpg2k_tile_5.tif");
    slideio::vsi::VSIFile vsiFile(filePath);
    auto etsFile = vsiFile.getEtsFile(0);
    cv::Mat tileRaster;
    etsFile->readTile(0, 0, { 0 }, 5, 0, tileRaster);
    //TestTools::showRaster(tileRaster);
    //ImageTools::writeTiffImage(testFilePath, tileRaster);
    cv::Mat testRaster;
    ImageTools::readGDALImage(testFilePath, testRaster);
    TestTools::compareRasters(testRaster, tileRaster);
    //TestTools::showRasters(testRaster, tileRaster);
}


TEST(Pyramid, init) {
    TestDimensionOrder dimOrder;
    {
        std::vector<std::tuple<int, int, int, int, int, int>> tls = {
            {0, 0, 0, 1, 1, 0},
            {0, 0, 1, 1, 1, 0},
            {0, 0, 0, 1, 1, 1},
            {0, 0, 1, 1, 1, 1},
            {1, 0, 0, 1, 1, 0},
            {1, 0, 1, 1, 1, 0},
            {1, 0, 0, 1, 1, 1},
            {1, 0, 1, 1, 1, 1},

            {0, 0, 0, 0, 1, 0},
            {0, 0, 1, 0, 1, 0},
            {0, 0, 0, 0, 1, 1},
            {0, 0, 1, 0, 1, 1},
            {1, 0, 0, 0, 1, 0},
            {1, 0, 1, 0, 1, 0},
            {1, 0, 0, 0, 1, 1},
            {1, 0, 1, 0, 1, 1},

            {0, 0, 0, 1, 0, 0},
            {0, 0, 1, 1, 0, 0},
            {0, 0, 0, 1, 0, 1},
            {0, 0, 1, 1, 0, 1},
            {1, 0, 0, 1, 0, 0},
            {1, 0, 1, 1, 0, 0},
            {1, 0, 0, 1, 0, 1},
            {1, 0, 1, 1, 0, 1},

            {0, 0, 0, 0, 0, 0},
            {0, 0, 1, 0, 0, 0},
            {0, 0, 0, 0, 0, 1},
            {0, 0, 1, 0, 0, 1},
            {1, 0, 0, 0, 0, 0},
            {1, 0, 1, 0, 0, 0},
            {1, 0, 0, 0, 0, 1},
            {1, 0, 1, 0, 0, 1},

            {0, 1, 0, 1, 1, 0},
            {0, 1, 1, 1, 1, 0},
            {0, 1, 0, 1, 1, 1},
            {0, 1, 1, 1, 1, 1},
            {1, 1, 0, 1, 1, 0},
            {1, 1, 1, 1, 1, 0},
            {1, 1, 0, 1, 1, 1},
            {1, 1, 1, 1, 1, 1},

            {0, 1, 0, 0, 1, 0},
            {0, 1, 1, 0, 1, 0},
            {0, 1, 0, 0, 1, 1},
            {0, 1, 1, 0, 1, 1},
            {1, 1, 0, 0, 1, 0},
            {1, 1, 1, 0, 1, 0},
            {1, 1, 0, 0, 1, 1},
            {1, 1, 1, 0, 1, 1},

            {0, 1, 0, 1, 0, 0},
            {0, 1, 1, 1, 0, 0},
            {0, 1, 0, 1, 0, 1},
            {0, 1, 1, 1, 0, 1},
            {1, 1, 0, 1, 0, 0},
            {1, 1, 1, 1, 0, 0},
            {1, 1, 0, 1, 0, 1},
            {1, 1, 1, 1, 0, 1},

            {0, 1, 0, 0, 0, 0},
            {0, 1, 1, 0, 0, 0},
            {0, 1, 0, 0, 0, 1},
            {0, 1, 1, 0, 0, 1},
            {1, 1, 0, 0, 0, 0},
            {1, 1, 1, 0, 0, 0},
            {1, 1, 0, 0, 0, 1},
            {1, 1, 1, 0, 0, 1}
        };
        std::vector<slideio::vsi::TileInfo> tiles;
        for (auto& t : tls) {
            slideio::vsi::TileInfo tile;
            tile.coordinates = {
                std::get<0>(t), std::get<1>(t), std::get<2>(t),
                std::get<3>(t), std::get<4>(t), std::get<5>(t)
            };
            tiles.push_back(tile);
        }
        vsi::Pyramid pyramid;
        pyramid.init(tiles, cv::Size(100, 100), cv::Size(10, 10), &dimOrder);
        EXPECT_EQ(2, pyramid.getNumLevels());
        EXPECT_EQ(2, pyramid.getNumChannelIndices());
        EXPECT_EQ(2, pyramid.getNumZIndices());
        EXPECT_EQ(2, pyramid.getNumTIndices());
        for (int lv = 0; lv < pyramid.getNumLevels(); ++lv) {
            const auto& level = pyramid.getLevel(lv);
            const int scaleLevel = 1 << lv;
            EXPECT_EQ(scaleLevel, level.getScaleLevel());
            EXPECT_EQ(cv::Size(100 >> lv, 100 >> lv), level.getSize());
            EXPECT_EQ(4, level.getNumTiles());
            for (int tileIndex = 0; tileIndex < level.getNumTiles(); ++tileIndex) {
                for (int channelIndex = 0; channelIndex < pyramid.getNumChannelIndices(); ++channelIndex) {
                    for (int zIndex = 0; zIndex < pyramid.getNumZIndices(); ++zIndex) {
                        for (int tIndex = 0; tIndex < pyramid.getNumTIndices(); ++tIndex) {
                            const int y = tileIndex / 2;
                            const int x = tileIndex % 2;
                            auto tile = level.getTile(tileIndex, channelIndex, zIndex, tIndex);
                            EXPECT_EQ(lv, tile.coordinates[5]);
                            EXPECT_EQ(x, tile.coordinates[0]);
                            EXPECT_EQ(y, tile.coordinates[1]);
                            EXPECT_EQ(channelIndex, tile.coordinates[2]);
                            EXPECT_EQ(zIndex, tile.coordinates[3]);
                            EXPECT_EQ(tIndex, tile.coordinates[4]);
                        }
                    }
                }
            }
        }
    }
}

TEST(Pyramid, init2DWithChannel) {
    TestDimensionOrder dimOrder(2, -1, -1);
    {
        std::vector<std::tuple<int, int, int, int>> tls = {
            {0, 0, 0, 0},
            {0, 0, 1, 0},
            {0, 0, 0, 1},
            {0, 0, 1, 1},
            {1, 0, 0, 0},
            {1, 0, 1, 0},
            {1, 0, 0, 1},
            {1, 0, 1, 1},

            {0, 1, 0, 0},
            {0, 1, 1, 0},
            {0, 1, 0, 1},
            {0, 1, 1, 1},
            {1, 1, 0, 0},
            {1, 1, 1, 0},
            {1, 1, 0, 1},
            {1, 1, 1, 1}
        };
        std::vector<slideio::vsi::TileInfo> tiles;
        for (auto& t : tls) {
            slideio::vsi::TileInfo tile;
            tile.coordinates = {
                std::get<0>(t), std::get<1>(t), std::get<2>(t),
                std::get<3>(t)
            };
            tiles.push_back(tile);
        }
        vsi::Pyramid pyramid;
        pyramid.init(tiles, cv::Size(100, 100), cv::Size(10, 10), &dimOrder);
        EXPECT_EQ(2, pyramid.getNumLevels());
        EXPECT_EQ(2, pyramid.getNumChannelIndices());
        EXPECT_EQ(1, pyramid.getNumZIndices());
        EXPECT_EQ(1, pyramid.getNumTIndices());
        for (int lv = 0; lv < pyramid.getNumLevels(); ++lv) {
            const auto& level = pyramid.getLevel(lv);
            const int scaleLevel = 1 << lv;
            EXPECT_EQ(scaleLevel, level.getScaleLevel());
            EXPECT_EQ(cv::Size(100 >> lv, 100 >> lv), level.getSize());
            EXPECT_EQ(4, level.getNumTiles());
            for (int tileIndex = 0; tileIndex < level.getNumTiles(); ++tileIndex) {
                for (int channelIndex = 0; channelIndex < pyramid.getNumChannelIndices(); ++channelIndex) {
                    for (int zIndex = 0; zIndex < pyramid.getNumZIndices(); ++zIndex) {
                        for (int tIndex = 0; tIndex < pyramid.getNumTIndices(); ++tIndex) {
                            const int y = tileIndex / 2;
                            const int x = tileIndex % 2;
                            auto tile = level.getTile(tileIndex, channelIndex, zIndex, tIndex);
                            EXPECT_EQ(lv, tile.coordinates[3]);
                            EXPECT_EQ(x, tile.coordinates[0]);
                            EXPECT_EQ(y, tile.coordinates[1]);
                            EXPECT_EQ(channelIndex, tile.coordinates[2]);
                        }
                    }
                }
            }
        }
    }
}

TEST(Pyramid, init3D) {
    TestDimensionOrder dimOrder(-1, 2, -1);
    {
        std::vector<std::tuple<int, int, int, int>> tls = {
            {0, 0, 0, 0},
            {0, 0, 1, 0},
            {0, 0, 0, 1},
            {0, 0, 1, 1},
            {1, 0, 0, 0},
            {1, 0, 1, 0},
            {1, 0, 0, 1},
            {1, 0, 1, 1},

            {0, 1, 0, 0},
            {0, 1, 1, 0},
            {0, 1, 0, 1},
            {0, 1, 1, 1},
            {1, 1, 0, 0},
            {1, 1, 1, 0},
            {1, 1, 0, 1},
            {1, 1, 1, 1}
        };
        std::vector<slideio::vsi::TileInfo> tiles;
        for (auto& t : tls) {
            slideio::vsi::TileInfo tile;
            tile.coordinates = {
                std::get<0>(t), std::get<1>(t), std::get<2>(t),
                std::get<3>(t)
            };
            tiles.push_back(tile);
        }
        vsi::Pyramid pyramid;
        pyramid.init(tiles, cv::Size(100, 100), cv::Size(10, 10), &dimOrder);
        EXPECT_EQ(2, pyramid.getNumLevels());
        EXPECT_EQ(1, pyramid.getNumChannelIndices());
        EXPECT_EQ(2, pyramid.getNumZIndices());
        EXPECT_EQ(1, pyramid.getNumTIndices());
        for (int lv = 0; lv < pyramid.getNumLevels(); ++lv) {
            const auto& level = pyramid.getLevel(lv);
            const int scaleLevel = 1 << lv;
            EXPECT_EQ(scaleLevel, level.getScaleLevel());
            EXPECT_EQ(cv::Size(100 >> lv, 100 >> lv), level.getSize());
            EXPECT_EQ(4, level.getNumTiles());
            for (int tileIndex = 0; tileIndex < level.getNumTiles(); ++tileIndex) {
                for (int channelIndex = 0; channelIndex < pyramid.getNumChannelIndices(); ++channelIndex) {
                    for (int zIndex = 0; zIndex < pyramid.getNumZIndices(); ++zIndex) {
                        for (int tIndex = 0; tIndex < pyramid.getNumTIndices(); ++tIndex) {
                            const int y = tileIndex / 2;
                            const int x = tileIndex % 2;
                            auto tile = level.getTile(tileIndex, channelIndex, zIndex, tIndex);
                            EXPECT_EQ(lv, tile.coordinates[3]);
                            EXPECT_EQ(x, tile.coordinates[0]);
                            EXPECT_EQ(y, tile.coordinates[1]);
                            EXPECT_EQ(zIndex, tile.coordinates[2]);
                        }
                    }
                }
            }
        }
    }
}
