// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <fstream>
#include <boost/format.hpp>
#include <gtest/gtest.h>
#include <opencv2/imgproc.hpp>


#include "slideio/slideio/imagedrivermanager.hpp"
#include "slideio/drivers/vsi/vsiimagedriver.hpp"

using namespace slideio;

TEST(VSIImageDriver, DriverManager_getDriverIDs)
{
    std::vector<std::string> driverIds = slideio::ImageDriverManager::getDriverIDs();
    auto it = std::find(driverIds.begin(),driverIds.end(), "VSI");
    EXPECT_FALSE(it==driverIds.end());
}

TEST(VSIImageDriver, getID)
{
    slideio::VSIImageDriver driver;
    const std::string id = driver.getID();
    EXPECT_EQ(id,"VSI");
}

TEST(VSIImageDriver, canOpenFile)
{
    slideio::VSIImageDriver driver;
    EXPECT_TRUE(driver.canOpenFile("c:\\abbb\\a.vsi"));
    EXPECT_FALSE(driver.canOpenFile("c:\\abbb\\a.zvi.tmp"));
}

