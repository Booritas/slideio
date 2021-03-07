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
    EXPECT_EQ(scene->getName(), "1.2.276.0.7230010.3.1.4.1787169844.28773.1454574501.602007");
}