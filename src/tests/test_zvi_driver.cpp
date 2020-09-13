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
    }
}
