#include <gtest/gtest.h>
#include "slideio/drivers/svs/svstools.hpp"
#include <string>
#include <regex>

static std::string description = "Aperio Image Library v11.2.1\n"
    "46000x32914 [42673,5576 2220x2967] (240x240) JPEG/RGB Q=30;Aperio Image Library v10.0.51\n"
    "46920x33014 [0,100 46000x32914] (256x256) JPEG/RGB Q=30|AppMag = 20|StripeWidth = 2040"
    "|ScanScope ID = CPAPERIOCS|Filename = CMU-1|Date = 12/29/09|Time = 09:59:15"
    "|User = b414003d-95c6-48b0-9369-8010ed517ba7|Parmset = USM Filter|MPP = 0.4990"
    "|Left = 25.691574|Top = 23.449873|LineCameraSkew = -0.000424"
    "|LineAreaXOffset = 0.019265|LineAreaYOffset = -0.000313"
    "|Focus Offset = 0.000000|ImageID = 1004486|OriginalWidth = 46920"
    "|Originalheight = 33014|Filtered = 5"
    "|OriginalWidth = 46000|OriginalHeight = 32914";

TEST(SVSTools, extractMagnification)
{
	int magn = slideio::SVSTools::extractMagnifiation(description);
	EXPECT_EQ(20, magn);
	std::string description2 = "91574|Top = 23.449873|LineCameraSkew = -0.000424";
	magn = slideio::SVSTools::extractMagnifiation(description2);
	EXPECT_EQ(0, magn);
	magn = slideio::SVSTools::extractMagnifiation("");
	EXPECT_EQ(0, magn);
}

TEST(SVSTools, extractResolution)
{
	double res = slideio::SVSTools::extractResolution(description);
	EXPECT_DOUBLE_EQ(0.499e-6, res);
	std::string description2 = "91574|Top = 23.449873|LineCameraSkew = -0.000424";
	res = slideio::SVSTools::extractResolution(description2);
	EXPECT_DOUBLE_EQ(0, res);
	res = slideio::SVSTools::extractResolution("");
	EXPECT_DOUBLE_EQ(0, res);
}

