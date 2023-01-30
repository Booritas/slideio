#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include "slideio/converter/convertertools.hpp"


TEST(ConverterTools, computeNumZoomLevels)
{
    EXPECT_EQ(slideio::ConverterTools::computeNumZoomLevels(100, 100),1);
    EXPECT_EQ(slideio::ConverterTools::computeNumZoomLevels(100000, 100), 1);
    EXPECT_EQ(slideio::ConverterTools::computeNumZoomLevels(1000, 100), 1);
    EXPECT_EQ(slideio::ConverterTools::computeNumZoomLevels(1000, 1000), 1);
    EXPECT_EQ(slideio::ConverterTools::computeNumZoomLevels(1001, 1001), 2);
    EXPECT_EQ(slideio::ConverterTools::computeNumZoomLevels(16000, 16000), 5);
}
