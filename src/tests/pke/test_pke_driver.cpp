#include <random>
#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include <string>
#include <tinyxml2.h>
#include <opencv2/imgproc.hpp>

#include "slideio/core/tools/tools.hpp"
#include "slideio/drivers/pke/pkeimagedriver.hpp"
#include "slideio/drivers/pke/pkescene.hpp"
#include "slideio/drivers/pke/pkeslide.hpp"
#include "slideio/imagetools/smallimage.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"
#include "slideio/slideio/slide.hpp"
#include "slideio/slideio/slideio.hpp"


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
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
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

TEST_F(PKEImageDriverTests, openSlideAutoDriver) {
    std::string filePath = TestTools::getFullTestImagePath("pke", "openmicroscopy/PKI_scans/HandEcompressed_Scan1.qptiff");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(filePath, "AUTO");
    ASSERT_TRUE(slide != nullptr);
    ASSERT_EQ(1, slide->getNumScenes());
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);
    EXPECT_EQ("HandEcompressed", scene->getName());
    EXPECT_EQ(std::make_tuple(0, 0, 30720, 26640), scene->getRect());
}

TEST_F(PKEImageDriverTests, openFLFile) {
    std::string filePath = TestTools::getFullTestImagePath("pke", "openmicroscopy/PKI_scans/LuCa-7color_Scan1.qptiff");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
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
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    std::string testFilePath = TestTools::getFullTestImagePath("pke", "test-images/HandEcompressed_Scan1 (1, x=11190, y=8580, w=1622, h=963).png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath);
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
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    std::string testFilePath = TestTools::getFullTestImagePath("pke", "test-images/LuCa-7color_Scan1.qptiff - resolution #1 (1, x=4981, y=10654, w=2367, h=1578).tif");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath);
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
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
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

// An auxiliary image (Thumbnail/Overview/Label) is a PKESmallScene: a single directory with
// no pyramid. It still has to report the one level it is so it can be addressed by level like
// every other scene, and reading that level has to match an ordinary readBlock.
TEST_F(PKEImageDriverTests, auxImageSingleZoomLevelAndLevelRead) {
    std::string filePath = TestTools::getFullTestImagePath("pke", "openmicroscopy/PKI_scans/LuCa-7color_Scan1.qptiff");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::PKEImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    std::shared_ptr<CVScene> thumbnail = slide->getAuxImage("Thumbnail");
    ASSERT_TRUE(thumbnail != nullptr);

    ASSERT_EQ(1, thumbnail->getNumZoomLevels());
    const LevelInfo* level = thumbnail->getZoomLevelInfo(0);
    ASSERT_TRUE(level != nullptr);
    EXPECT_EQ(0, level->getLevel());
    EXPECT_DOUBLE_EQ(1.0, level->getScale());
    const cv::Rect sceneRect = thumbnail->getRect();
    EXPECT_EQ(sceneRect.width, level->getSize().width);
    EXPECT_EQ(sceneRect.height, level->getSize().height);

    cv::Mat viaBlock, viaLevel;
    thumbnail->readBlock(sceneRect, viaBlock);
    thumbnail->readResampledLevelBlockChannels(0, sceneRect, sceneRect.size(), {}, viaLevel);
    ASSERT_EQ(viaBlock.size(), viaLevel.size());
    ASSERT_EQ(viaBlock.type(), viaLevel.type());
    cv::Mat diff;
    cv::absdiff(viaBlock, viaLevel, diff);
    EXPECT_EQ(0, cv::countNonZero(diff.reshape(1)));
}

TEST_F(PKEImageDriverTests, metadata) {
    std::string filePath = TestTools::getFullTestImagePath("pke", "openmicroscopy/PKI_scans/LuCa-7color_Scan1.qptiff");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
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
	EXPECT_EQ(slide->getMetadataFormat(), slideio::MetadataFormat::XML);
	auto scene = slide->getScene(0);
	ASSERT_TRUE(scene != nullptr);
	EXPECT_EQ(scene->getMetadataFormat(), slideio::MetadataFormat::None);
}


TEST_F(PKEImageDriverTests, readStripedDir) {
    std::string filePath = TestTools::getFullTestImagePath("pke", "openmicroscopy/PKI_scans/HandEcompressed_Scan1.qptiff");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    std::string testFilePath = TestTools::getFullTestImagePath("pke", "test-images/HandEcompressed_Scan1-low.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath);
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
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    std::string testFilePath = TestTools::getFullTestImagePath("pke", "test-images/LuCa-7color_Scan1-low.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath);
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
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    std::string testFilePath = TestTools::getFullTestImagePath("pke", "test-images/LuCa-7color_Scan1-low.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath);
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
        SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath);
        //TestTools::writePNG(channelRaster, testFilePath);
        cv::Mat referenceRaster;
        TestTools::readPNG(testFilePath, referenceRaster);
        TestTools::compareRasters(referenceRaster, channelRaster);
    }
}

TEST_F(PKEImageDriverTests, readStripedDir5ChannelsAllChannels) {
    std::string filePath = TestTools::getFullTestImagePath("pke", "openmicroscopy/PKI_scans/LuCa-7color_Scan1.qptiff");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
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
        SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath);
        //TestTools::writePNG(channelRaster, testFilePath);
        cv::Mat referenceRaster;
        TestTools::readPNG(testFilePath, referenceRaster);
        TestTools::compareRasters(referenceRaster, channelRaster);
    }
}

TEST_F(PKEImageDriverTests, readMultichannelImageNoScaleAllChannels) {
    std::string filePath = TestTools::getFullTestImagePath("pke", "openmicroscopy/PKI_scans/LuCa-7color_Scan1.qptiff");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    std::string refImagePath = TestTools::getFullTestImagePath("pke", "test-images/LuCa-7color_Scan1.qptiff - resolution #1 (1, x=11619, y=16875, w=1202, h=756).tif");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(refImagePath);
    cv::Mat refImage;
    ImageTools::openSmallImage(refImagePath)->readImageStack(refImage);
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
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    std::string refImagePath = TestTools::getFullTestImagePath("pke", "test-images/LuCa-7color_Scan1.qptiff - resolution #1 (1, x=11619, y=16875, w=1202, h=756).tif");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(refImagePath);
    cv::Mat refImage;
    ImageTools::openSmallImage(refImagePath)->readImageStack(refImage);
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
    std::string filePath = TestTools::getFullTestImagePath("pke", "openmicroscopy/PKI_scans/LuCa-7color_Scan1.qptiff");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::PKEImageDriver driver;
    TestTools::multiThreadedTest(filePath, driver);
}

TEST_F(PKEImageDriverTests, getDriverId)
{
    std::string filePath = TestTools::getFullTestImagePath("pke", "openmicroscopy/PKI_scans/LuCa-7color_Scan1.qptiff");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
	auto slide = slideio::openSlide(filePath, "AUTO");
    ASSERT_TRUE(slide);
    EXPECT_EQ("QPTIFF", slide->getDriverId());
    const int numScenes = slide->getNumScenes();
    EXPECT_EQ(1, numScenes);
    for (int iScene = 0; iScene < numScenes; ++iScene) {
        auto scene = slide->getScene(iScene);
        EXPECT_TRUE(scene.get() != nullptr);
		auto cvScene = scene->getCVScene();
        EXPECT_EQ(iScene, cvScene->getSceneIndex());
        EXPECT_EQ(filePath, cvScene->getFilePath());
		EXPECT_EQ("QPTIFF", cvScene->getDriverId());
    }
}

TEST_F(PKEImageDriverTests, readLevelMatchesTheResampledSceneRead) {
    // computeSimilarity2 goes through cv::sum, which only supports up to 4 channels, so this
    // uses the 3-channel brightfield fixture rather than the 5-channel fluorescent one.
    std::string filePath = TestTools::getFullTestImagePath("pke", "openmicroscopy/PKI_scans/HandEcompressed_Scan1.qptiff");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::PKEImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);

    const int numLevels = scene->getNumZoomLevels();
    ASSERT_LE(2, numLevels);
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
    // An overhanging rect is the ordinary edge tile and must come back background filled.
    const slideio::LevelInfo* last = scene->getZoomLevelInfo(numLevels - 1);
    const cv::Size lastSize(last->getSize().width, last->getSize().height);
    cv::Mat edge;
    ASSERT_NO_THROW(scene->readResampledLevelBlockChannels(
        numLevels - 1, cv::Rect(lastSize.width - 32, lastSize.height - 32, 128, 128),
        cv::Size(128, 128), {}, edge));
    EXPECT_EQ(cv::Size(128, 128), edge.size());
}

// The generic CVScene default clamps levelRect to the level and then re-derives a zoom
// index from the requested output size; when that re-derived index happens to agree with
// the level actually asked for, the default's output is byte-identical to a direct read of
// that other level. So reading level 0 resampled down to level 1's size must NOT come back
// identical to a native read of level 1 -- equality would mean level 0's read was actually
// served by level 1.
TEST_F(PKEImageDriverTests, readLevelDoesNotReuseAdjacentLevel) {
    // Uses the 5-channel fluorescent fixture (not the brightfield one the sibling similarity
    // test needs): this is a multiplex image, so a wrong-level read would exercise PKE's
    // one-directory-per-channel path (dir.channels == 1) where the level->directory
    // indirection meets a second per-channel offset -- the interaction this test guards.
    std::string filePath = TestTools::getFullTestImagePath("pke", "openmicroscopy/PKI_scans/LuCa-7color_Scan1.qptiff");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::PKEImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);
    ASSERT_LE(2, scene->getNumZoomLevels());

    const slideio::LevelInfo* level0 = scene->getZoomLevelInfo(0);
    const slideio::LevelInfo* level1 = scene->getZoomLevelInfo(1);
    ASSERT_TRUE(level0 != nullptr);
    ASSERT_TRUE(level1 != nullptr);
    const cv::Size size0(level0->getSize().width, level0->getSize().height);
    const cv::Size size1(level1->getSize().width, level1->getSize().height);

    // Read level 0 resampled to level 1's size -- must be served from level 0 directly, not
    // silently reselected to level 1 by the generic base default.
    cv::Mat viaLevel0Resampled;
    scene->readResampledLevelBlockChannels(0, cv::Rect(cv::Point(0, 0), size0), size1, {}, viaLevel0Resampled);
    // Read level 1 natively: no resampling at all.
    cv::Mat viaLevel1Native;
    scene->readResampledLevelBlockChannels(1, cv::Rect(cv::Point(0, 0), size1), size1, {}, viaLevel1Native);

    ASSERT_EQ(size1, viaLevel0Resampled.size());
    ASSERT_EQ(size1, viaLevel1Native.size());
    // The two are independently encoded streams; equality means level 0's read was actually
    // served from level 1.
    EXPECT_GT(cv::norm(viaLevel0Resampled, viaLevel1Native, cv::NORM_INF), 0);
}
