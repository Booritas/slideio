#include <gtest/gtest.h>
#include "slideio/drivers/svs/svstools.hpp"
#include <string>
#include <regex>
#include <locale>
#include <stdexcept>

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

static std::string descriptionMPPWithComa = "Aperio Image Library v11.2.1\n"
    "46000x32914 [42673,5576 2220x2967] (240x240) JPEG/RGB Q=30;Aperio Image Library v10.0.51\n"
    "46920x33014 [0,100 46000x32914] (256x256) JPEG/RGB Q=30|AppMag = 20|StripeWidth = 2040"
    "|ScanScope ID = CPAPERIOCS|Filename = CMU-1|Date = 12/29/09|Time = 09:59:15"
    "|User = b414003d-95c6-48b0-9369-8010ed517ba7|Parmset = USM Filter|MPP = 0,235"
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

// Philips names the magnification of a zoom level in the description of the tiff
// directory holding it: "level=1 mag=20 quality=80". Level 0 carries the philips xml
// instead and never names one, so the magnification of the slide has to be derived
// from a level below it: a level covers 2^-level of the base, so the base is
// mag * 2^level.
TEST(SVSTools, extractPhilipsMagnificationDerivesTheBaseFromALevel)
{
	EXPECT_DOUBLE_EQ(40., slideio::SVSTools::extractPhilipsMagnification("level=1 mag=20 quality=80"));
}

// The magnification of a level is fractional from level 1 down on a scanner whose
// base is not a power of two multiple, e.g. the 44x of Philips-1.tiff.
TEST(SVSTools, extractPhilipsMagnificationReadsFractionalValues)
{
	EXPECT_DOUBLE_EQ(44., slideio::SVSTools::extractPhilipsMagnification("level=3 mag=5.5 quality=80"));
	EXPECT_DOUBLE_EQ(41., slideio::SVSTools::extractPhilipsMagnification("level=1 mag=20.5 quality=80"));
}

TEST(SVSTools, extractPhilipsMagnificationScalesByTheLevelNumber)
{
	EXPECT_DOUBLE_EQ(40., slideio::SVSTools::extractPhilipsMagnification("level=8 mag=0.15625 quality=80"));
	// A description that names level 0 needs no scaling.
	EXPECT_DOUBLE_EQ(40., slideio::SVSTools::extractPhilipsMagnification("level=0 mag=40 quality=80"));
}

TEST(SVSTools, extractPhilipsMagnificationReturnsZeroWhenThereIsNone)
{
	EXPECT_DOUBLE_EQ(0., slideio::SVSTools::extractPhilipsMagnification(""));
	// The description of an auxiliary directory of a philips file.
	EXPECT_DOUBLE_EQ(0., slideio::SVSTools::extractPhilipsMagnification(
		"Macro -offset=(0,0)-pixelsize=(0.0315,0.0315)-rois=(0,0,1816,821)"));
	// An aperio description: the magnification is there, in another syntax.
	EXPECT_DOUBLE_EQ(0., slideio::SVSTools::extractPhilipsMagnification(description));
	EXPECT_DOUBLE_EQ(0., slideio::SVSTools::extractPhilipsMagnification("level=1 quality=80"));
	EXPECT_DOUBLE_EQ(0., slideio::SVSTools::extractPhilipsMagnification("mag=20 quality=80"));
}

TEST(SVSTools, extractPhilipsMagnificationRejectsUnusableValues)
{
	EXPECT_DOUBLE_EQ(0., slideio::SVSTools::extractPhilipsMagnification("level=1 mag=0 quality=80"));
	EXPECT_DOUBLE_EQ(0., slideio::SVSTools::extractPhilipsMagnification("level=x mag=y quality=80"));
	// A level number no pyramid can reach: scaling by it would overflow to infinity.
	EXPECT_DOUBLE_EQ(0., slideio::SVSTools::extractPhilipsMagnification("level=9999 mag=20 quality=80"));
	// A level number too long to fit an int must be rejected like any other unusable
	// value: a corrupt description must not stop the file from opening.
	EXPECT_DOUBLE_EQ(0., slideio::SVSTools::extractPhilipsMagnification(
		"level=99999999999999999999 mag=20 quality=80"));
}

// The value is read from a file, so it must not depend on the locale the embedding
// application happens to have set: under a comma decimal locale a host that honours
// the global locale reads "5.5" as 5, and the slide comes out at 40x instead of 44x.
TEST(SVSTools, extractPhilipsMagnificationIsIndependentOfTheHostLocale)
{
	const std::locale original = std::locale();
	bool imbued = false;
	for (const char* name : {"de-DE", "de_DE.UTF-8", "German_Germany.1252"}) {
		try {
			std::locale::global(std::locale(name));
			imbued = true;
			break;
		}
		catch (const std::runtime_error&) {
		}
	}
	if (!imbued) {
		GTEST_SKIP() << "No comma decimal locale is installed on this machine";
	}
	const double magnification = slideio::SVSTools::extractPhilipsMagnification("level=3 mag=5.5 quality=80");
	std::locale::global(original);
	EXPECT_DOUBLE_EQ(44., magnification);
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
	res = slideio::SVSTools::extractResolution(descriptionMPPWithComa);
	EXPECT_DOUBLE_EQ(0.235e-6, res);
}


