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

// A philips level directory names its own level: "level=1 mag=20 quality=80". The base
// level's directory carries the xml metadata instead and names none.
TEST(SVSTools, extractPhilipsLevelNumberReadsTheLevel)
{
	EXPECT_EQ(1, slideio::SVSTools::extractPhilipsLevelNumber("level=1 mag=20 quality=80"));
	EXPECT_EQ(8, slideio::SVSTools::extractPhilipsLevelNumber("level=8 mag=0.15625 quality=80"));
}

TEST(SVSTools, extractPhilipsLevelNumberReturnsMinusOneWhenThereIsNone)
{
	EXPECT_EQ(-1, slideio::SVSTools::extractPhilipsLevelNumber(""));
	EXPECT_EQ(-1, slideio::SVSTools::extractPhilipsLevelNumber("interloper"));
	EXPECT_EQ(-1, slideio::SVSTools::extractPhilipsLevelNumber(
		"Macro -offset=(0,0)-pixelsize=(0.0315,0.0315)"));
	// The aperio syntax names no philips level.
	EXPECT_EQ(-1, slideio::SVSTools::extractPhilipsLevelNumber(description));
	// "levels=" is not "level=" -- the derivation description of a converted file
	// contains "levels=10003,10002" and must not be read as a level number.
	EXPECT_EQ(-1, slideio::SVSTools::extractPhilipsLevelNumber("levels=10003,10002 mag=20"));
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



TEST(SVSTools, aperioDateTimeToEpochSeconds)
{
    // Aperio states the scan time as two properties of the image description,
    // "Date = 12/29/09" and "Time = 09:59:15" -- month first, and a year of two
    // digits. Bio-Formats reads the same pair with "MM/dd/yy HH:mm:ss"
    // (SVSReader.DATE_FORMAT).
    ASSERT_TRUE(slideio::SVSTools::aperioDateTimeToEpochSeconds("12/29/09", "09:59:15", "").has_value());
    EXPECT_EQ(*slideio::SVSTools::aperioDateTimeToEpochSeconds("12/29/09", "09:59:15", ""), 1262080755LL);
    EXPECT_EQ(*slideio::SVSTools::aperioDateTimeToEpochSeconds("07/16/09", "18:15:06", ""), 1247768106LL);
    EXPECT_EQ(*slideio::SVSTools::aperioDateTimeToEpochSeconds("01/13/10", "11:58:28", ""), 1263383908LL);
    // A four digit year is taken as it stands.
    EXPECT_EQ(*slideio::SVSTools::aperioDateTimeToEpochSeconds("12/29/2009", "09:59:15", ""), 1262080755LL);
}

TEST(SVSTools, aperioDateTimeAppliesTheStatedTimeZone)
{
    // "Time Zone = GMT-05:00" is a property Aperio writes and no file in the
    // corpus carries, so the offset is exercised here rather than through one.
    // Without it the text names a local time the file does not qualify, and it
    // is read as UTC.
    EXPECT_EQ(*slideio::SVSTools::aperioDateTimeToEpochSeconds("12/29/09", "09:59:15", "GMT-05:00"),
              1262080755LL + 5 * 3600);
    EXPECT_EQ(*slideio::SVSTools::aperioDateTimeToEpochSeconds("12/29/09", "09:59:15", "GMT+02:00"),
              1262080755LL - 2 * 3600);
    EXPECT_EQ(*slideio::SVSTools::aperioDateTimeToEpochSeconds("12/29/09", "09:59:15", "GMT+0200"),
              1262080755LL - 2 * 3600);
}

TEST(SVSTools, aperioDateTimeRejectsWhatItCannotRead)
{
    EXPECT_FALSE(slideio::SVSTools::aperioDateTimeToEpochSeconds("", "09:59:15", "").has_value());
    EXPECT_FALSE(slideio::SVSTools::aperioDateTimeToEpochSeconds("12/29/09", "", "").has_value());
    EXPECT_FALSE(slideio::SVSTools::aperioDateTimeToEpochSeconds("29/12/09", "09:59:15", "").has_value());
    EXPECT_FALSE(slideio::SVSTools::aperioDateTimeToEpochSeconds("12-29-09", "09:59:15", "").has_value());
    EXPECT_FALSE(slideio::SVSTools::aperioDateTimeToEpochSeconds("12/29/09", "25:59:15", "").has_value());
    EXPECT_FALSE(slideio::SVSTools::aperioDateTimeToEpochSeconds("yesterday", "09:59:15", "").has_value());
}

TEST(SVSTools, aperioTwoDigitYearUsesAFixedWindow)
{
    // 00-68 is 2000-2068 and 69-99 is 1969-1999, the rule strptime's %y uses.
    // Java's SimpleDateFormat, which Bio-Formats parses this with, slides its
    // window with the current date instead, so the two libraries will disagree
    // about a year far enough out -- and this one at least answers the same way
    // next decade as it does today.
    ASSERT_TRUE(slideio::SVSTools::aperioDateTimeToEpochSeconds("01/01/68", "00:00:00", "").has_value());
    EXPECT_EQ(*slideio::SVSTools::aperioDateTimeToEpochSeconds("01/01/68", "00:00:00", ""), 3092601600LL);
    EXPECT_EQ(*slideio::SVSTools::aperioDateTimeToEpochSeconds("01/01/69", "00:00:00", ""), -31536000LL);
    EXPECT_EQ(*slideio::SVSTools::aperioDateTimeToEpochSeconds("01/01/70", "00:00:00", ""), 0LL);
}

TEST(SVSTools, significantBitsFromAcquisitionBitDepth)
{
    // Aperio states it in the same header the scan time comes from. The survey
    // that first recorded SVS as stating no significant bits looked only at the
    // TIFF tags and missed this, and jp2k_1chnl.svs is the file that shows it:
    // 10 bits of a 16 bit sample.
    const std::string withDepth =
        "Aperio Image Library v10.2.20\n1600x1721 [0,100 1559x1621] (256x256) J2K/KDU Q=70"
        "|AppMag = 20|Date = 01/13/10|Time = 11:58:28|Dye = Alexa Fluor 488"
        "|Exposure Time = 800|Acquisition Bit Depth = 10";
    EXPECT_EQ(slideio::SVSTools::significantBitsFromDescription(withDepth, 16), 10);

    const std::string withoutDepth =
        "Aperio Image Library v11.2.1\n46000x32914 (256x256) JPEG/RGB Q=30"
        "|AppMag = 20|Date = 12/29/09|Time = 09:59:15";
    EXPECT_EQ(slideio::SVSTools::significantBitsFromDescription(withoutDepth, 8), 0);
    EXPECT_EQ(slideio::SVSTools::significantBitsFromDescription("", 8), 0);
    EXPECT_EQ(slideio::SVSTools::significantBitsFromDescription(
        "Aperio Image Library\nlabel 387x463", 8), 0);
    // Not a number, and a negative, are both refused.
    EXPECT_EQ(slideio::SVSTools::significantBitsFromDescription(
        "Aperio\nx|Acquisition Bit Depth = deep", 16), 0);
    EXPECT_EQ(slideio::SVSTools::significantBitsFromDescription(
        "Aperio\nx|Acquisition Bit Depth = -4", 16), 0);
}

TEST(SVSTools, aperioDateTimeFallsBackToUtcOnAnUnreadableZone)
{
    // A zone the parser cannot spell-match must not cost the Date and Time,
    // which were perfectly readable. Dropping them would discard more of the
    // file than the branch above, which reads an absent zone as UTC.
    ASSERT_TRUE(slideio::SVSTools::aperioDateTimeToEpochSeconds("12/29/09", "09:59:15", "GMT").has_value());
    EXPECT_EQ(*slideio::SVSTools::aperioDateTimeToEpochSeconds("12/29/09", "09:59:15", "GMT"), 1262080755LL);
    EXPECT_EQ(*slideio::SVSTools::aperioDateTimeToEpochSeconds("12/29/09", "09:59:15", "Eastern"), 1262080755LL);
}

TEST(SVSTools, significantBitsNeverExceedTheSampleTheyDescribe)
{
    // Aperio's "Acquisition Bit Depth" is the camera's, and a camera wider than
    // the samples a scan was written with is not a contradiction in the scanner
    // -- it is one in what the two numbers together would mean. 10 significant
    // bits of an 8 bit sample is not something a caller can shift by, so it
    // reports the getter's word for unknown instead.
    const std::string tenBits =
        "Aperio Image Library v10.2.20\n1600x1721|AppMag = 20|Acquisition Bit Depth = 10";
    EXPECT_EQ(slideio::SVSTools::significantBitsFromDescription(tenBits, 16), 10);
    EXPECT_EQ(slideio::SVSTools::significantBitsFromDescription(tenBits, 8), 0);

    // Equal is not a contradiction: a file may state that every stored bit
    // carries data, and that is a statement rather than a default.
    const std::string eightBits =
        "Aperio Image Library v10.2.20\n1600x1721|AppMag = 20|Acquisition Bit Depth = 8";
    EXPECT_EQ(slideio::SVSTools::significantBitsFromDescription(eightBits, 8), 8);

    // Where the storage width is not known there is nothing to contradict, so
    // the stated value stands.
    EXPECT_EQ(slideio::SVSTools::significantBitsFromDescription(tenBits, 0), 10);
}
