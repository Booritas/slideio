// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/drivers/czi/czitools.hpp"
#include <tinyxml2.h>

int testPixelType(char *value)
{
	std::string bgrData = std::string("<PixelType>") + value + std::string("Bgr</PixelType>");
	tinyxml2::XMLDocument xmlDoc;
	xmlDoc.Parse(bgrData.c_str());
	tinyxml2::XMLElement* xmlElem = xmlDoc.RootElement();
	int count = CZITools::channelCountFromPixelType(xmlElem);
	return count;
}

TEST(CZITools, channelCountFromPixelType)
{
	EXPECT_EQ(testPixelType("Gray8"), 1);
	EXPECT_EQ(testPixelType("Gray16"), 1);
	EXPECT_EQ(testPixelType("Bgr24"), 3);
	EXPECT_EQ(testPixelType("Bgra32"), 4);
	EXPECT_EQ(testPixelType("Gray32Float"), 1);
	EXPECT_EQ(testPixelType("Bgr48"), 3);
	EXPECT_EQ(testPixelType("Bgr96Float"), 3);
	EXPECT_EQ(testPixelType("Gray64ComplexFloat"), 1);
	EXPECT_EQ(testPixelType("Gray32Float"), 1);
	EXPECT_EQ(testPixelType("Bgr192ComplexFloat"), 3);
}
