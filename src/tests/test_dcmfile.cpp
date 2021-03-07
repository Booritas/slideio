// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/drivers/dcm/dcmfile.hpp"
#include "testtools.hpp"

using namespace  slideio;

TEST(DCMFile, init)
{
    std::string slidePath = TestTools::getTestImagePath("dcm", "benigns_01/patient0186/0186.LEFT_CC.dcm");
    DCMFile file(slidePath);
    file.init();
    const int width = file.getWidth();
    const int height = file.getHeight();
    const int numSlices = file.getNumSlices();
    const std::string seriesUID = file.getSeriesUID();
    EXPECT_EQ(width, 3984);
    EXPECT_EQ(height, 5528);
    EXPECT_EQ(numSlices, 1);
    EXPECT_EQ(seriesUID, std::string("1.2.276.0.7230010.3.1.4.1787169844.28773.1454574501.602007"));
    EXPECT_EQ(1, file.getNumChannels());

}