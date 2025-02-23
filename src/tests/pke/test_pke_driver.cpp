﻿#include <random>
#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include <string>
#include <tinyxml2.h>
#include <opencv2/imgproc.hpp>

#include "slideio/core/tools/tools.hpp"
#include "slideio/drivers/pke/pkeimagedriver.hpp"
#include "slideio/drivers/pke/pkescene.hpp"
#include "slideio/drivers/pke/pkeslide.hpp"
#include "slideio/core/imagedrivermanager.hpp"


namespace slideio
{
    class Slide;
}

using namespace slideio;


class PKEImageDriverTests : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ImageDriverManager::setLogLevel("WARNING");
        std::cerr << "SetUpTestSuite: Running before all tests\n";
    }
    static void TearDownTestSuite() {
    }
};

TEST_F(PKEImageDriverTests, openBrightFieldFile) {
    std::string filePath = TestTools::getFullTestImagePath("pke","openmicroscopy/PKI_scans/HandEcompressed_Scan1.qptiff");
    slideio::PKEImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide!=nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(1, numScenes);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    EXPECT_DOUBLE_EQ(20., scene->getMagnification());
    EXPECT_EQ("HandEcompressed", scene->getName());
    cv::Rect rectScene = scene->getRect();
    EXPECT_EQ(cv::Rect(0,0,30720,26640),rectScene);
    EXPECT_EQ(3, scene->getNumChannels());
    EXPECT_EQ(DataType::DT_Byte, scene->getChannelDataType(0));
    Resolution res = scene->getResolution();
    EXPECT_DOUBLE_EQ(4.9889322681313341e-07, res.x);
    EXPECT_DOUBLE_EQ(4.9889322681313341e-07, res.y);
    EXPECT_EQ(3, slide->getNumAuxImages());
    EXPECT_FALSE(slide->getRawMetadata().empty());
    std::list<std::string> auxNames = slide->getAuxImageNames();
    std::list<std::string> expectedAuxNames = {"Thumbnail", "Overview", "Label"};
    EXPECT_EQ(expectedAuxNames, auxNames);
    EXPECT_EQ(Compression::Jpeg, scene->getCompression());
    const int zoomLevels = scene->getNumZoomLevels();
    EXPECT_EQ(5, zoomLevels);
    for(int zoomLevel=0; zoomLevel<zoomLevels; zoomLevel++) {
        const LevelInfo* levelInfo = scene->getZoomLevelInfo(zoomLevel);
        EXPECT_DOUBLE_EQ(20./(1<<zoomLevel), levelInfo->getMagnification());
        Size size1(30720 / (1 << zoomLevel), 26640 / (1 << zoomLevel));
        Size size2 = levelInfo->getSize();
        EXPECT_EQ(size1, size2);
        EXPECT_DOUBLE_EQ(1. / (1 << zoomLevel), levelInfo->getScale());
        if(zoomLevel<4) {
            EXPECT_EQ(Size(512,512),levelInfo->getTileSize());
        }
        else {
            EXPECT_EQ(Size(0,0), levelInfo->getTileSize());
        }
    }
}

TEST_F(PKEImageDriverTests, openFLFile) {
    std::string filePath = TestTools::getFullTestImagePath("pke", "openmicroscopy/PKI_scans/LuCa-7color_Scan1.qptiff");
    slideio::PKEImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(1, numScenes);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    EXPECT_DOUBLE_EQ(10., scene->getMagnification());
    EXPECT_EQ("LuCa-7color", scene->getName());
    cv::Rect rectScene = scene->getRect();
    EXPECT_EQ(cv::Rect(0, 0, 24960, 34560), rectScene);
    EXPECT_EQ(5, scene->getNumChannels());
    EXPECT_EQ(DataType::DT_Byte, scene->getChannelDataType(0));
    Resolution res = scene->getResolution();
    EXPECT_DOUBLE_EQ(4.9799450221850718e-07, res.x);
    EXPECT_DOUBLE_EQ(4.9799450221850718e-07, res.y);
    EXPECT_EQ(3, slide->getNumAuxImages());
    EXPECT_FALSE(slide->getRawMetadata().empty());
    std::list<std::string> auxNames = slide->getAuxImageNames();
    std::list<std::string> expectedAuxNames = { "Thumbnail", "Overview", "Label" };
    EXPECT_EQ(expectedAuxNames, auxNames);
    EXPECT_EQ(Compression::LZW, scene->getCompression());
    const int zoomLevels = scene->getNumZoomLevels();
    EXPECT_EQ(6, zoomLevels);
    for (int zoomLevel = 0; zoomLevel < zoomLevels; zoomLevel++) {
        const LevelInfo* levelInfo = scene->getZoomLevelInfo(zoomLevel);
        EXPECT_DOUBLE_EQ(10. / (1 << zoomLevel), levelInfo->getMagnification());
        EXPECT_EQ(Size(rectScene.width / (1 << zoomLevel), rectScene.height / (1 << zoomLevel)), levelInfo->getSize());
        EXPECT_DOUBLE_EQ(1. / (1 << zoomLevel), levelInfo->getScale());
        if (zoomLevel < (zoomLevels-1)) {
            EXPECT_EQ(Size(512, 512), levelInfo->getTileSize());
        }
        else {
            EXPECT_EQ(Size(0, 0), levelInfo->getTileSize());
        }
    }
    const std::list<std::string> expectedChannelNames = { "DAPI","FITC","CY3","Texas Red", "CY5" };
    std::list<std::string> channelNames;
    for(int channelIndex=0; channelIndex<scene->getNumChannels(); channelIndex++) {
        channelNames.push_back(scene->getChannelName(channelIndex));
    }
    EXPECT_EQ(expectedChannelNames, channelNames);

}

TEST_F(PKEImageDriverTests, readBrightFieldRegion) {
    std::string filePath = TestTools::getFullTestImagePath("pke", "openmicroscopy/PKI_scans/HandEcompressed_Scan1.qptiff");
    std::string testFilePath = TestTools::getFullTestImagePath("pke", "test-images/HandEcompressed_Scan1 (1, x=11190, y=8580, w=1622, h=963).png");
    slideio::PKEImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(1, numScenes);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    cv::Rect rectRoi = { 11190, 8580, 1622, 963 };
    cv::Mat raster;
    scene->readBlock(rectRoi, raster);
    cv::Mat testRaster;
    TestTools::readPNG(testFilePath, testRaster);
    TestTools::compareRasters(raster, testRaster);
    cv::Size resampledSize = rectRoi.size();
    double scale = 0.333;
    resampledSize.width = static_cast<int>(resampledSize.width*scale);
    resampledSize.height = static_cast<int>(resampledSize.height*scale);
    scene->readResampledBlock(rectRoi, resampledSize, raster);
    cv::resize(testRaster, testRaster, resampledSize);
    double similarity = ImageTools::computeSimilarity2(raster, testRaster);
    //TestTools::showRasters(raster, testRaster);
    EXPECT_GE(similarity, 0.92);
}

TEST_F(PKEImageDriverTests, readFLRegion) {
    std::string filePath = TestTools::getFullTestImagePath("pke", "openmicroscopy/PKI_scans/LuCa-7color_Scan1.qptiff");
    std::string testFilePath = TestTools::getFullTestImagePath("pke", "test-images/LuCa-7color_Scan1.qptiff - resolution #1 (1, x=4981, y=10654, w=2367, h=1578).tif");
    slideio::PKEImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(1, numScenes);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    cv::Rect rectRoi = { 4981, 10654, 2367, 1578 };
    cv::Mat raster;
    const std::vector<int> channelIndices = { 0 };
    scene->readBlockChannels(rectRoi, channelIndices, raster);
    cv::Mat testRaster;
    TestTools::readTiffDirectories(testFilePath, channelIndices, testRaster);
    TestTools::compareRasters(raster, testRaster);
    //TestTools::showRasters(raster, testRaster);
    cv::Size resampledSize = rectRoi.size();
    double scale = 0.333;
    resampledSize.width = static_cast<int>(resampledSize.width * scale);
    resampledSize.height = static_cast<int>(resampledSize.height * scale);
    cv::Mat resampledRaster;
    scene->readResampledBlockChannels(rectRoi, resampledSize, channelIndices, resampledRaster);
    cv::Mat testRasterResampled;
    cv::resize(testRaster, testRasterResampled, resampledSize);
    double similarity = ImageTools::computeSimilarity2(resampledRaster, testRasterResampled);
    //TestTools::showRasters(resampledRaster, testRasterResampled);
    EXPECT_GE(similarity, 0.99);
}

void testAuxImage(std::shared_ptr<CVSlide>& slide, const std::string& filePath, const std::string& auxName) {
    auto thumbnail = slide->getAuxImage(auxName);
    cv::Mat auxRaster;
    thumbnail->readBlock(thumbnail->getRect(), auxRaster);
    //TestTools::writePNG(auxRaster, filePath);
    cv::Mat auxTestRaster;
    TestTools::readPNG(filePath, auxTestRaster);
    //TestTools::showRaster(auxTestRaster);
    TestTools::compareRasters(auxRaster, auxTestRaster);
}

TEST_F(PKEImageDriverTests, auxiliaryImages) {
    std::string filePath = TestTools::getFullTestImagePath("pke", "openmicroscopy/PKI_scans/LuCa-7color_Scan1.qptiff");
    const std::list<std::string> auxPaths = {
        TestTools::getFullTestImagePath("pke", "test-images/LuCa-7color_Scan1.thumb.png"),
        TestTools::getFullTestImagePath("pke", "test-images/LuCa-7color_Scan1.overv.png"),
        TestTools::getFullTestImagePath("pke", "test-images/LuCa-7color_Scan1.label.png")
    };

    slideio::PKEImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(1, numScenes);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    std::list<std::string> expectedAuxNames = { "Thumbnail", "Overview", "Label" };
    std::list<std::string> auxNames = slide->getAuxImageNames();
    EXPECT_EQ(expectedAuxNames, auxNames);
    auto auxPath = auxPaths.begin();
    auto auxName = auxNames.begin();
    while(auxPath!=auxPaths.end() && auxName!=auxNames.end()) {
        testAuxImage(slide, *auxPath, *auxName);
        ++auxPath;
        ++auxName;
    }
}

TEST_F(PKEImageDriverTests, metadata) {
    std::string filePath = TestTools::getFullTestImagePath("pke", "openmicroscopy/PKI_scans/LuCa-7color_Scan1.qptiff");
    slideio::PKEImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    std::string metadata = slide->getRawMetadata();
    ASSERT_FALSE(metadata.empty());
    tinyxml2::XMLDocument doc;
    doc.Parse(metadata.c_str(), metadata.size());
    auto root = doc.RootElement();
    int count = 0;
    for(auto child=root->FirstChildElement(); child!=nullptr; child=child->NextSiblingElement()) {
        std::string name = child->Name();
        EXPECT_EQ(name, "PerkinElmer-QPI-ImageDescription");
        ++count;
    }
    EXPECT_EQ(5, count);
}


TEST_F(PKEImageDriverTests, readStripedDir) {
    std::string filePath = TestTools::getFullTestImagePath("pke", "openmicroscopy/PKI_scans/HandEcompressed_Scan1.qptiff");
    std::string testFilePath = TestTools::getFullTestImagePath("pke", "test-images/HandEcompressed_Scan1-low.png");
    slideio::PKEImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(1, numScenes);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    cv::Rect rectRoi = scene->getRect();
    cv::Mat raster;
    cv::Size size = { 500, 500 };
    const double scale = 500. / rectRoi.width;
    size.height = static_cast<int>(rectRoi.height*scale);

    scene->readResampledBlock(rectRoi, size, raster);
    cv::Mat testRaster;
    //TestTools::writePNG(raster, testFilePath);
    TestTools::readPNG(testFilePath, testRaster);
    TestTools::compareRasters(raster, testRaster);
    //TestTools::showRasters(raster, testRaster);
}

TEST_F(PKEImageDriverTests, readStripedDir5Channels_SelectedChannels) {
    std::string filePath = TestTools::getFullTestImagePath("pke", "openmicroscopy/PKI_scans/LuCa-7color_Scan1.qptiff");
    std::string testFilePath = TestTools::getFullTestImagePath("pke", "test-images/LuCa-7color_Scan1-low.png");
    slideio::PKEImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(1, numScenes);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    cv::Rect rectRoi = scene->getRect();
    cv::Mat raster;
    cv::Size size = { 500, 500 };
    const double scale = 500. / rectRoi.width;
    size.height = static_cast<int>(rectRoi.height * scale);
    const int channels = scene->getNumChannels();
    ASSERT_EQ(channels, 5);

    scene->readResampledBlockChannels(rectRoi, size, { 0,1,2 }, raster);
    cv::Mat testRaster;
    //TestTools::writePNG(raster, testFilePath);
    TestTools::readPNG(testFilePath, testRaster);
    TestTools::compareRasters(raster, testRaster);
    //TestTools::writePNG(raster, "/tmp/test.png");
    //TestTools::showRasters(raster, testRaster);
}

TEST_F(PKEImageDriverTests, readStripedDir5Channels_SingleChannel) {
    std::string filePath = TestTools::getFullTestImagePath("pke", "openmicroscopy/PKI_scans/LuCa-7color_Scan1.qptiff");
    std::string testFilePath = TestTools::getFullTestImagePath("pke", "test-images/LuCa-7color_Scan1-low.png");
    slideio::PKEImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(1, numScenes);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    cv::Rect rectRoi = scene->getRect();
    cv::Mat raster;
    cv::Size size = { 500, 500 };
    const double scale = 500. / rectRoi.width;
    size.height = static_cast<int>(rectRoi.height * scale);
    const int channels = scene->getNumChannels();
    ASSERT_EQ(channels, 5);
    for (int channel = 0; channel < channels; ++channel) {
        cv::Mat channelRaster;
        scene->readResampledBlockChannels(rectRoi, size, { channel }, channelRaster);
        std::string fileName = "test-images/LuCa-7color_Scan1-low-" + std::to_string(channel) + ".png";
        std::string testFilePath = TestTools::getFullTestImagePath("pke", fileName);
        //TestTools::writePNG(channelRaster, testFilePath);
        cv::Mat referenceRaster;
        TestTools::readPNG(testFilePath, referenceRaster);
        TestTools::compareRasters(referenceRaster, channelRaster);
    }
}

TEST_F(PKEImageDriverTests, readStripedDir5ChannelsAllChannels) {
    std::string filePath = TestTools::getFullTestImagePath("pke", "openmicroscopy/PKI_scans/LuCa-7color_Scan1.qptiff");
    slideio::PKEImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(1, numScenes);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    cv::Rect rectRoi = scene->getRect();
    cv::Mat raster;
    cv::Size size = { 500, 500 };
    const double scale = 500. / rectRoi.width;
    size.height = static_cast<int>(rectRoi.height * scale);
    const int channels = scene->getNumChannels();
    ASSERT_EQ(channels, 5);

    scene->readResampledBlock(rectRoi, size, raster);
    for(int channel=0; channel<channels; ++channel) {
        cv::Mat channelRaster;
        cv::extractChannel(raster, channelRaster, channel);
        std::string fileName = "test-images/LuCa-7color_Scan1-low-" + std::to_string(channel) + ".png";
        std::string testFilePath = TestTools::getFullTestImagePath("pke", fileName);
        //TestTools::writePNG(channelRaster, testFilePath);
        cv::Mat referenceRaster;
        TestTools::readPNG(testFilePath, referenceRaster);
        TestTools::compareRasters(referenceRaster, channelRaster);
    }
}

TEST_F(PKEImageDriverTests, readMultichannelImageNoScaleAllChannels) {
    std::string filePath = TestTools::getFullTestImagePath("pke", "openmicroscopy/PKI_scans/LuCa-7color_Scan1.qptiff");
    std::string refImagePath = TestTools::getFullTestImagePath("pke", "test-images/LuCa-7color_Scan1.qptiff - resolution #1 (1, x=11619, y=16875, w=1202, h=756).tif");
    cv::Mat refImage;
    ImageTools::readGDALImage(refImagePath, refImage);
    ASSERT_EQ(refImage.size(), cv::Size(1202, 756));
    ASSERT_EQ(refImage.channels(), 5);
    slideio::PKEImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(1, numScenes);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    cv::Rect rectScene = scene->getRect();

    cv::Rect rectRoi = { 11619, 16875, 1202, 756 };
    cv::Mat channel_0;
    cv::Size size = rectRoi.size();
    const int channels = scene->getNumChannels();
    ASSERT_EQ(channels, 5);

    cv::Mat roi;
    scene->readBlock(rectRoi, roi);
    TestTools::compareRasters(roi, refImage);
}

TEST_F(PKEImageDriverTests, readMultichannelImageNoScaleSeparatedChannels) {
    std::string filePath = TestTools::getFullTestImagePath("pke", "openmicroscopy/PKI_scans/LuCa-7color_Scan1.qptiff");
    std::string refImagePath = TestTools::getFullTestImagePath("pke", "test-images/LuCa-7color_Scan1.qptiff - resolution #1 (1, x=11619, y=16875, w=1202, h=756).tif");
    cv::Mat refImage;
    ImageTools::readGDALImage(refImagePath, refImage);
    ASSERT_EQ(refImage.size(), cv::Size(1202, 756));
    ASSERT_EQ(refImage.channels(), 5);
    slideio::PKEImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(1, numScenes);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    cv::Rect rectScene = scene->getRect();

    cv::Rect rectRoi = { 11619, 16875, 1202, 756 };
    cv::Mat channel_0;
    cv::Size size = rectRoi.size();
    const int channels = scene->getNumChannels();
    ASSERT_EQ(channels, 5);

    for (int channel = 0; channel < channels; ++channel) {
        cv::Mat channelRaster;
        scene->readBlockChannels(rectRoi, { channel }, channelRaster);
        cv::Mat channelRef;
        cv::extractChannel(refImage, channelRef, channel);
        TestTools::compareRasters(channelRaster, channelRef);
    }
}



TEST_F(PKEImageDriverTests, multiThreadSceneAccess) {
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() <<
            "Skip the test because full dataset is not enabled";
    }
    std::string filePath = TestTools::getFullTestImagePath("pke", "openmicroscopy/PKI_scans/LuCa-7color_Scan1.qptiff");
    slideio::PKEImageDriver driver;
    TestTools::multiThreadedTest(filePath, driver);
}