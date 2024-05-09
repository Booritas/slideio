#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include <string>
#include <opencv2/imgproc.hpp>

#include "slideio/core/tools/tools.hpp"
#include "slideio/drivers/pke/pkeimagedriver.hpp"
#include "slideio/drivers/pke/pkescene.hpp"
#include "slideio/drivers/pke/pkeslide.hpp"


namespace slideio
{
    class Slide;
}

using namespace slideio;


TEST(PKEImageDriver, openBrightFieldFile) {
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
        EXPECT_EQ(cv::Size(30720 / (1 << zoomLevel), 26640 / (1 << zoomLevel)), levelInfo->getSize());
        EXPECT_DOUBLE_EQ(1. / (1 << zoomLevel), levelInfo->getScale());
        if(zoomLevel<4) {
            EXPECT_EQ(cv::Size(512,512),levelInfo->getTileSize());
        }
        else {
            EXPECT_EQ(cv::Size(0,0), levelInfo->getTileSize());
        }
    }
}

TEST(PKEImageDriver, openFLFile) {
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
        EXPECT_EQ(cv::Size(rectScene.width / (1 << zoomLevel), rectScene.height / (1 << zoomLevel)), levelInfo->getSize());
        EXPECT_DOUBLE_EQ(1. / (1 << zoomLevel), levelInfo->getScale());
        if (zoomLevel < (zoomLevels-1)) {
            EXPECT_EQ(cv::Size(512, 512), levelInfo->getTileSize());
        }
        else {
            EXPECT_EQ(cv::Size(0, 0), levelInfo->getTileSize());
        }
    }
    const std::list<std::string> expectedChannelNames = { "DAPI","FITC","CY3","Texas Red", "CY5" };
    std::list<std::string> channelNames;
    for(int channelIndex=0; channelIndex<scene->getNumChannels(); channelIndex++) {
        channelNames.push_back(scene->getChannelName(channelIndex));
    }
    EXPECT_EQ(expectedChannelNames, channelNames);

}
