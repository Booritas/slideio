// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/core/imagedrivermanager.hpp"
#include "testtools.hpp"
#include "slideio/scene.hpp"
#include "slideio/drivers/zvi/zviimagedriver.hpp"

TEST(ZVIImageDriver, DriverManager_getDriverIDs)
{
    std::vector<std::string> driverIds = slideio::ImageDriverManager::getDriverIDs();
    auto it = std::find(driverIds.begin(),driverIds.end(), "ZVI");
    EXPECT_FALSE(it==driverIds.end());
}

TEST(ZVIImageDriver, getID)
{
    slideio::ZVIImageDriver driver;
    const std::string id = driver.getID();
    EXPECT_EQ(id,"ZVI");
}

TEST(ZVIImageDriver, canOpenFile)
{
    slideio::ZVIImageDriver driver;
    EXPECT_TRUE(driver.canOpenFile("c:\\abbb\\a.zvi"));
    EXPECT_FALSE(driver.canOpenFile("c:\\abbb\\a.zvi.tmp"));
}

TEST(ZVIImageDriver, openSlide2D)
{
    slideio::ZVIImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Merged.zvi");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide.get() != nullptr);
    const int sceneCount = slide->getNumScenes();
    ASSERT_EQ(sceneCount, 1);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene.get() != nullptr);
    const auto rect = scene->getRect();
    EXPECT_EQ(rect.x, 0);
    EXPECT_EQ(rect.y, 0);
    EXPECT_EQ(rect.width, 1480);
    EXPECT_EQ(rect.height, 1132);
    EXPECT_EQ(scene->getNumChannels(), 3);
    EXPECT_EQ(scene->getNumZSlices(), 1);
    EXPECT_EQ(scene->getNumTFrames(), 1);
    EXPECT_EQ(scene->getChannelDataType(0), slideio::DataType::DT_Int16);
    EXPECT_EQ(scene->getChannelDataType(1), slideio::DataType::DT_Int16);
    EXPECT_EQ(scene->getChannelDataType(2), slideio::DataType::DT_Int16);
    EXPECT_EQ(scene->getChannelName(0), std::string("Hoechst 33342"));
    EXPECT_EQ(scene->getChannelName(1), std::string("Cy3"));
    EXPECT_EQ(scene->getChannelName(2), std::string("FITC"));
    auto res = scene->getResolution();
    EXPECT_DOUBLE_EQ(res.x, 0.0645e-6);
    EXPECT_DOUBLE_EQ(res.y, 0.0645e-6);
}

TEST(ZVIImageDriver, openSlide3D)
{
    slideio::ZVIImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Stacked.zvi");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide.get() != nullptr);
    const int sceneCount = slide->getNumScenes();
    ASSERT_EQ(sceneCount, 1);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene.get() != nullptr);
    const auto rect = scene->getRect();
    EXPECT_EQ(rect.x, 0);
    EXPECT_EQ(rect.y, 0);
    EXPECT_EQ(rect.width, 1388);
    EXPECT_EQ(rect.height, 1040);
    EXPECT_EQ(scene->getNumChannels(), 3);
    EXPECT_EQ(scene->getNumZSlices(), 13);
    EXPECT_EQ(scene->getNumTFrames(), 1);
    EXPECT_EQ(scene->getChannelDataType(0), slideio::DataType::DT_Int16);
    EXPECT_EQ(scene->getChannelDataType(1), slideio::DataType::DT_Int16);
    EXPECT_EQ(scene->getChannelDataType(2), slideio::DataType::DT_Int16);
    EXPECT_EQ(scene->getChannelName(0), std::string("Hoechst 33342"));
    EXPECT_EQ(scene->getChannelName(1), std::string("Cy3"));
    EXPECT_EQ(scene->getChannelName(2), std::string("FITC"));
    auto res = scene->getResolution();
    EXPECT_DOUBLE_EQ(res.x, 0.0645e-6);
    EXPECT_DOUBLE_EQ(res.y, 0.0645e-6);
    auto zres = scene->getZSliceResolution();
    EXPECT_DOUBLE_EQ(zres, 0.25e-6);
}

TEST(ZVIImageDriver, openSlideMosaic)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because private dataset is not enabled";
    }

    slideio::ZVIImageDriver driver;
    std::string filePath = TestTools::getFullTestImagePath("zvi", "Zeiss-3-Mosaic.zvi");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide.get() != nullptr);
    const int sceneCount = slide->getNumScenes();
    ASSERT_EQ(sceneCount, 1);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene.get() != nullptr);
    const auto rect = scene->getRect();
    EXPECT_EQ(rect.x, 0);
    EXPECT_EQ(rect.y, 0);
    EXPECT_EQ(rect.width, 13882);
    EXPECT_EQ(rect.height, 21631);
    EXPECT_EQ(scene->getNumChannels(), 3);
    EXPECT_EQ(scene->getNumZSlices(), 1);
    EXPECT_EQ(scene->getNumTFrames(), 1);
    EXPECT_EQ(scene->getChannelDataType(0), slideio::DataType::DT_Int16);
    EXPECT_EQ(scene->getChannelDataType(1), slideio::DataType::DT_Int16);
    EXPECT_EQ(scene->getChannelDataType(2), slideio::DataType::DT_Int16);
    EXPECT_EQ(scene->getChannelName(0), std::string("Hoechst 33342"));
    EXPECT_EQ(scene->getChannelName(1), std::string("Alexa 488"));
    EXPECT_EQ(scene->getChannelName(2), std::string("Cy3"));
    auto res = scene->getResolution();
    EXPECT_DOUBLE_EQ(res.x, 0.3225e-6);
    EXPECT_DOUBLE_EQ(res.y, 0.3225e-6);
    auto zres = scene->getZSliceResolution();
    EXPECT_DOUBLE_EQ(zres, 1);
}
