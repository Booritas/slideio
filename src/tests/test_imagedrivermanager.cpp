#include <gtest/gtest.h>
#include "slideio/slideio/imagedrivermanager.hpp"
#include "slideio/slideio/slideio.hpp"


GTEST_TEST(ImageDriverManager, getDriverIDs)
{
    std::vector<std::string> driverIDs = slideio::ImageDriverManager::getDriverIDs();
    EXPECT_FALSE(driverIDs.empty());
}

GTEST_TEST(ImageDriverManager, getDriversGlobal)
{
    auto drivers = slideio::getDriverIDs();
    EXPECT_FALSE(drivers.empty());
}
