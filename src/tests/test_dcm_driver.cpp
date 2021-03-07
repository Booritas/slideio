// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/core/imagedrivermanager.hpp"
#include "testtools.hpp"
#include "slideio/scene.hpp"
#include "slideio/drivers/dcm/dcmimagedriver.hpp"

using namespace  slideio;

TEST(DCMImageDriver, DriverManager_getDriverIDs)
{
    std::vector<std::string> driverIds = ImageDriverManager::getDriverIDs();
    auto it = std::find(driverIds.begin(),driverIds.end(), "DCM");
    EXPECT_FALSE(it==driverIds.end());
}
TEST(DCMImageDriver, getID)
{
    DCMImageDriver driver;
    std::string id = driver.getID();
    EXPECT_EQ(id,"DCM");
}

TEST(DCMImageDriver, canOpenFile)
{
    DCMImageDriver driver;
    EXPECT_TRUE(driver.canOpenFile("c:\\abbb\\a.dcm"));
    EXPECT_FALSE(driver.canOpenFile("c:\\abbb\\a.scn.tmp"));
}

TEST(DCMImageDriver, openFile)
{
    DCMImageDriver driver;
    std::string slidePath = TestTools::getTestImagePath("dcm", "benigns_01/patient0186/0186.LEFT_CC.dcm");
    auto slide = driver.openFile(slidePath);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene);
    const cv::Rect rect = scene->getRect();
    const cv::Rect refRect = { 0, 0, 3984, 5528 };
    EXPECT_EQ(rect, refRect);
    const int numChannels = scene->getNumChannels();
    const int numSlices = scene->getNumZSlices();
    const int numFrames = scene->getNumTFrames();
    EXPECT_EQ(numChannels, 1);
    EXPECT_EQ(numSlices, 1);
    EXPECT_EQ(numFrames, 1);
    EXPECT_EQ(scene->getName(), "case0377");
}


TEST(DCMImageDriver, openDirectory)
{
    if (!TestTools::isPrivateTestEnabled()) {
        GTEST_SKIP() << "Skip private test because private dataset is not enabled";
    }
    DCMImageDriver driver;
    std::string slidePath = TestTools::getTestImagePath("dcm", "series/series_1", true);
    auto slide = driver.openFile(slidePath);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene);
    const cv::Rect rect = scene->getRect();
    const cv::Rect refRect = { 0, 0, 512, 512 };
    EXPECT_EQ(rect, refRect);
    const int numChannels = scene->getNumChannels();
    const int numSlices = scene->getNumZSlices();
    const int numFrames = scene->getNumTFrames();
    EXPECT_EQ(numChannels, 1);
    EXPECT_EQ(numSlices, 15);
    EXPECT_EQ(numFrames, 1);
    EXPECT_EQ(scene->getName(), "COU IV");
}


TEST(DCMImageDriver, openDirectoryRecursively)
{
    if (!TestTools::isPrivateTestEnabled()) {
        GTEST_SKIP() << "Skip private test because private dataset is not enabled";
    }
    DCMImageDriver driver;
    std::string slidePath = TestTools::getTestImagePath("dcm", "series", true);
    auto slide = driver.openFile(slidePath);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 2);
    auto scene = slide->getScene(0);
    const std::string sceneName = scene->getName();
    if(sceneName=="COU IV") {
        scene = slide->getScene(1);
    }
    ASSERT_TRUE(scene);
    const cv::Rect rect = scene->getRect();
    const cv::Rect refRect = { 0, 0, 512, 512 };
    EXPECT_EQ(rect, refRect);
    const int numChannels = scene->getNumChannels();
    const int numSlices = scene->getNumZSlices();
    const int numFrames = scene->getNumTFrames();
    EXPECT_EQ(numChannels, 1);
    EXPECT_EQ(numSlices, 9);
    EXPECT_EQ(numFrames, 1);
    EXPECT_EQ(scene->getName(), "1.2.276.0.7230010.3.100.1.1");
}