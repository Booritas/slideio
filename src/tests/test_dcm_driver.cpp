// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/core/imagedrivermanager.hpp"
#include "testtools.hpp"
#include "slideio/scene.hpp"

TEST(DCMImageDriver, DriverManager_getDriverIDs)
{
    std::vector<std::string> driverIds = slideio::ImageDriverManager::getDriverIDs();
    auto it = std::find(driverIds.begin(),driverIds.end(), "DCM");
    EXPECT_FALSE(it==driverIds.end());
}
//TEST(DCMImageDriver, getID)
//{
//    slideio::SCNImageDriver driver;
//    std::string id = driver.getID();
//    EXPECT_EQ(id,"DCM");
//}
//
//TEST(DCMImageDriver, canOpenFile)
//{
//    slideio::SCNImageDriver driver;
//    EXPECT_TRUE(driver.canOpenFile("c:\\abbb\\a.dcm"));
//    EXPECT_FALSE(driver.canOpenFile("c:\\abbb\\a.scn.tmp"));
//}
