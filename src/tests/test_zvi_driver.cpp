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

TEST(ZVIImageDriver, openSlide)
{
    const std::string images[] = {
        "Zeiss-1-Merged.zvi"
    };
    slideio::ZVIImageDriver driver;
    for (const auto& imageName : images)
    {
        std::string filePath = TestTools::getTestImagePath("zvi", imageName);
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
        EXPECT_EQ(scene->getNumChannels(), 1);
        EXPECT_EQ(scene->getChannelDataType(0), slideio::DataType::DT_Int16);
        EXPECT_EQ(scene->getChannelName(0), std::string("intensity"));
    }
}
