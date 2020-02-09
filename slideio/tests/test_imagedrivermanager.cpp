#include <gtest/gtest.h>
#include "slideio/core/imagedrivermanager.hpp"
#include "slideio/slideio.hpp"


GTEST_TEST(Slideio_ImageDriverManager, getDriverIDs)
{
    std::vector<std::string> driverIDs = cv::slideio::ImageDriverManager::getDriverIDs();
    EXPECT_FALSE(driverIDs.empty());
}

GTEST_TEST(Slideio_ImageDriverManager, getDriversGlobal)
{
    auto drivers = cv::slideio::getDrivers();
    EXPECT_FALSE(drivers.empty());
}
