#include <random>
#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include <string>
#include <opencv2/imgproc.hpp>
#include <filesystem>

#include "slideio/core/tools/tools.hpp"
#include "slideio/drivers/vsi/vsiimagedriver.hpp"
#include "slideio/drivers/vsi/vsiscene.hpp"
#include "slideio/drivers/vsi/vsislide.hpp"
#include "slideio/drivers/vsi/vsifile.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"


namespace slideio
{
    class Slide;
}

using namespace slideio;
namespace fso = std::filesystem;

class VSIImageDriverTests : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ImageDriverManager::setLogLevel("WARNING");
        std::cerr << "SetUpTestSuite: Running before all tests\n";
    }
    static void TearDownTestSuite() {
    }
};


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

TEST_F(VSIImageDriverTests, openFileWithExternalFiles1) {
    std::string filePath = TestTools::getFullTestImagePath("vsi","OS-1/OS-1.vsi");
    slideio::VSIImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide!=nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(1, numScenes);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    EXPECT_DOUBLE_EQ(20., scene->getMagnification());
    EXPECT_EQ(scene->getName(), "20x");
    auto rect = scene->getRect();
    std::string metadata = slide->getRawMetadata();
    EXPECT_GT(metadata.size(), 0);
}

TEST_F(VSIImageDriverTests, openFileWithoutExternalFiles) {
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

static std::shared_ptr<CVScene> getSceneByName(std::shared_ptr<CVSlide> slide, const std::string& name) {
	const int numScenes = slide->getNumScenes();
	for (int sceneIndex = 0; sceneIndex < numScenes; ++sceneIndex) {
		std::shared_ptr<CVScene> scene = slide->getScene(sceneIndex);
		if (scene->getName() == name) {
			return scene;
		}
	}
	return nullptr;
}

TEST_F(VSIImageDriverTests, openFileWithExternalFiles) {
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
        std::shared_ptr<CVScene> scene = getSceneByName(slide, std::get<0>(result[sceneIndex]));
		ASSERT_TRUE(scene != nullptr);
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

TEST_F(VSIImageDriverTests, auxImages) {
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


TEST_F(VSIImageDriverTests, VSIFileOpenWithOutExternalFiles) {
    std::string filePath = TestTools::getFullTestImagePath("vsi",
                                                           "Zenodo/Q6VM49JF/Figure-1-ultrasound-raw-data"
                                                           "/SPECTRUM_#201_2016-06-14_Jiangtao Liu/1286FL9057GDF8RGDX257R2GLHZ.vsi");
    vsi::VSIFile vsiFile(filePath);
    EXPECT_EQ(0, vsiFile.getNumEtsFiles());
}

TEST_F(VSIImageDriverTests, readVSISceneStripedDirUncompressed) {
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

TEST_F(VSIImageDriverTests, readVSISceneStripedDirUncompressedRoi) {
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

TEST_F(VSIImageDriverTests, readVSISceneStripedDirUncompressedRoiResampled) {
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

TEST_F(VSIImageDriverTests, VSIFileOpenWithExternalFiles) {
    std::string filePath = TestTools::getFullTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.vsi");
    vsi::VSIFile vsiFile(filePath);
    EXPECT_EQ(4, vsiFile.getNumEtsFiles());
}

TEST_F(VSIImageDriverTests, read3DVolume16bitSlice) {
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


TEST_F(VSIImageDriverTests, read3DVolume16bit) {
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
    ImageTools::readGDALImageSubDataset(testFilePath, 1,testRaster);
    cv::resize(testRaster, testRaster, blockSize);
    double similarity = ImageTools::computeSimilarity2(testRaster, blockRaster);
    EXPECT_GT(similarity, 0.99);
    //TestTools::showRasters(testRaster, blockRaster);
}

TEST_F(VSIImageDriverTests, read3DStack16bit) {
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
    ImageTools::readGDALImageSubDataset(testFilePath4, 1, testRaster);
    double similarity = ImageTools::computeSimilarity2(testRaster, slice);
    EXPECT_GT(similarity, 0.99);
    CVTools::extractSliceFrom3D(blockRaster, 1, slice);
    ImageTools::readGDALImageSubDataset(testFilePath5, 1, testRaster);
    EXPECT_GT(similarity, 0.99);
}

TEST_F(VSIImageDriverTests, readMultiscene) {
    std::string filePath = TestTools::getFullTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.vsi");
    std::string testFilePath = TestTools::getFullTestImagePath("vsi", "test-output/G1M16_ABD_HE_B6.vsi-40x_01(1,x=5836,y=11793,w=849,h=607).png");
    slideio::VSIImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(3, numScenes);
    std::string sceneName = "40x_01";
    std::shared_ptr<CVScene> scene = getSceneByName(slide, sceneName);
    ASSERT_TRUE(scene != nullptr);
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

TEST_F(VSIImageDriverTests, readMultisceneResized) {
    std::string filePath = TestTools::getFullTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.vsi");
    std::string testFilePath = TestTools::getFullTestImagePath("vsi", "test-output/G1M16_ABD_HE_B6.vsi-40x_01(1,x=5836,y=11793,w=849,h=607).png");
    slideio::VSIImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(3, numScenes);
    std::string sceneName = "40x_01";
    std::shared_ptr<CVScene> scene = getSceneByName(slide, sceneName);
    ASSERT_TRUE(scene != nullptr);
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

TEST_F(VSIImageDriverTests, readMultisceneResizedSingeChannel) {
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

TEST_F(VSIImageDriverTests, readMultisceneResizedReversedChannels) {
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
    const std::string dir("stack10001");
    const int numEtsFiles = vsiFile.getNumEtsFiles();
    std::shared_ptr<vsi::EtsFile> etsFile;
    for(int fileIndex=0; fileIndex<numEtsFiles; ++fileIndex) {
        auto ets = vsiFile.getEtsFile(fileIndex);
        std::string etsFilePath = ets->getFilePath();
        fso::path etsPath(etsFilePath);
        fso::path etsDir = etsPath.parent_path();
        std::string etsDirName = etsDir.filename().string();
        if(etsDirName == dir) {
            etsFile = ets;
        }
    }
    ASSERT_TRUE(etsFile.get() != nullptr);
    cv::Mat tileRaster;
    etsFile->readTile(0, 0, {},0, 0,  tileRaster);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    double score = ImageTools::computeSimilarity2(testRaster, tileRaster);
	EXPECT_GT(score, 0.999);
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
    double simScore = ImageTools::computeSimilarity2(testRaster, tileRaster);
    EXPECT_GT(simScore, 0.999);
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
        auto tiles = std::make_shared<vsi::TileInfoList>();
        for (auto& t : tls) {
            slideio::vsi::TileInfo tile;
            tile.coordinates = {
                std::get<0>(t), std::get<1>(t), std::get<2>(t),
                std::get<3>(t), std::get<4>(t), std::get<5>(t)
            };
            tiles->push_back(tile);
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
		auto tiles = std::make_shared<vsi::TileInfoList>();
        for (auto& t : tls) {
            slideio::vsi::TileInfo tile;
            tile.coordinates = {
                std::get<0>(t), std::get<1>(t), std::get<2>(t),
                std::get<3>(t)
            };
            tiles->push_back(tile);
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
		auto tiles = std::make_shared<vsi::TileInfoList>();
        for (auto& t : tls) {
            slideio::vsi::TileInfo tile;
            tile.coordinates = {
                std::get<0>(t), std::get<1>(t), std::get<2>(t),
                std::get<3>(t)
            };
            tiles->push_back(tile);
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

TEST_F(VSIImageDriverTests, invalidEts) {
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() <<
            "Skip the test because full dataset is not enabled";
    }
    std::string filePath = TestTools::getFullTestImagePath("vsi", "vs200-vsi-share/Image_B309.vsi");
    std::string overviewFilePath = TestTools::getFullTestImagePath("vsi", "test-output/Image_B309_Overview.png");
    std::string macroFilePath = TestTools::getFullTestImagePath("vsi", "test-output/Image_B309_Macro.png");
    slideio::VSIImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(0, numScenes);
    const int numAuxImages = slide->getNumAuxImages();
    ASSERT_EQ(2, numAuxImages);
    std::list<std::string> auxImageNames = slide->getAuxImageNames();
    ASSERT_EQ(2, auxImageNames.size());
    EXPECT_EQ("Macro image", auxImageNames.front());
    EXPECT_EQ("Overview", auxImageNames.back());
    std::shared_ptr<CVScene> macro = slide->getAuxImage(auxImageNames.front());
    ASSERT_TRUE(macro != nullptr);
    std::shared_ptr<CVScene> overview = slide->getAuxImage(auxImageNames.back());
    ASSERT_TRUE(overview != nullptr);
    cv::Mat macroRaster, macroTestRaster;
    cv::Rect rasterRect = macro->getRect();
    double cof = std::min(500./rasterRect.width, 500./rasterRect.height);
    cv::Size rasterSize(std::lround(cof*rasterRect.width), lround(cof*rasterRect.height));
    macro->readResampledBlock(rasterRect, rasterSize, macroRaster);
    //TestTools::writePNG(macroRaster, macroFilePath);
    TestTools::readPNG(macroFilePath, macroTestRaster);
    TestTools::compareRasters(macroRaster, macroTestRaster);
    //TestTools::showRaster(macroRaster);
    cv::Mat overviewRaster, overviewTestRaster;
    rasterRect = overview->getRect();
    cof = std::min(500. / rasterRect.width, 500. / rasterRect.height);
    rasterSize = { (int)std::lround(cof * rasterRect.width), (int)lround(cof * rasterRect.height) };
    overview->readResampledBlock(rasterRect, rasterSize, overviewRaster);
    //TestTools::writePNG(overviewRaster, overviewFilePath);
    TestTools::readPNG(overviewFilePath, overviewTestRaster);
    TestTools::compareRasters(overviewRaster, overviewTestRaster);
    //TestTools::showRaster(overviewRaster);

}

TEST_F(VSIImageDriverTests, volumes) {
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() <<
            "Skip the test because full dataset is not enabled";
    }
    std::string filePath = TestTools::getFullTestImagePath("vsi", "private/d/STS_G6889_11_1_pHH3.vsi");
    std::string testImageFilePath = TestTools::getFullTestImagePath("vsi", 
        "test-output/STS_G6889_11_1_pHH3.vsi - 40x_BF_01 (1, x=82570, y=77046, w=1153, h=797).png");
    slideio::VSIImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
	auto metadata = slide->getRawMetadata();
	ASSERT_FALSE(metadata.empty());
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(1, numScenes);
	auto scene = slide->getScene(0);
    auto sceneRect = scene->getRect();
	EXPECT_EQ(sceneRect, cv::Rect(0, 0, 164267, 150739));
	const cv::Rect roi = { 82570, 77046, 1153, 797 };
	const cv::Size blockSize = { roi.width, roi.height };
	cv::Mat raster;
	scene->readResampledBlock(roi, blockSize, raster);
	cv::Mat testRaster;
	TestTools::readPNG(testImageFilePath, testRaster);
	double similarity = ImageTools::computeSimilarity2(testRaster, raster);
	EXPECT_GT(similarity, 0.99);
}


TEST_F(VSIImageDriverTests, stack3d) {
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() <<
            "Skip the test because full dataset is not enabled";
    }
    std::string filePath = TestTools::getFullTestImagePath("vsi", "private/3d/01072022_35_2_z.vsi");
    std::string slice6 = TestTools::getFullTestImagePath("vsi", 
        "private/3d/test-images/01072022_35_2_z.vsi - 60x_BF_Z_01 (1, x=45625, y=42302, w=984, h=1015).png");
    slideio::VSIImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(1, numScenes);
    auto scene = slide->getScene(0);
    auto sceneRect = scene->getRect();
    EXPECT_EQ(sceneRect, cv::Rect(0,0,122351,76276));
	EXPECT_EQ(3, scene->getNumChannels());
    EXPECT_DOUBLE_EQ(60, scene->getMagnification());
	auto resolution = scene->getResolution();
    EXPECT_LT(std::fabs(0.0913e-6 - resolution.x), 1.e-9);
    EXPECT_LT(std::fabs(0.0913e-6 - resolution.y), 1.e-9);
    auto slices = scene->getNumZSlices();
	auto frames = scene->getNumTFrames();
	EXPECT_EQ(1, frames);
	EXPECT_EQ(13, slices);
	EXPECT_EQ(2, slide->getNumAuxImages());
    auto metadata = slide->getRawMetadata();
	EXPECT_FALSE(metadata.empty());
	cv::Mat raster;
	cv::Rect roi(45625, 42302, 984, 1015);
	cv::Size blockSize(roi.width, roi.height);
	scene->readResampled4DBlock(roi, blockSize, { 6,7 }, { 0,1 }, raster);
    cv::Mat testRaster;
	TestTools::readPNG(slice6, testRaster);
	double similarity = ImageTools::computeSimilarity2(testRaster, raster);
	EXPECT_GT(similarity, 0.99);
}

TEST_F(VSIImageDriverTests, multiThreadSceneAccess) {
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() <<
            "Skip the test because full dataset is not enabled";
    }
    std::string filePath = TestTools::getFullTestImagePath("vsi", "private/d/STS_G6889_11_1_pHH3.vsi");
    slideio::VSIImageDriver driver;
    TestTools::multiThreadedTest(filePath, driver);
}