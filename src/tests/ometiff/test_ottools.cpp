#include <gtest/gtest.h>
#include "slideio/drivers/ome-tiff/ottools.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"
#include <cmath>
#include <optional>
#include <tinyxml2.h>

using namespace slideio;
using namespace slideio::ometiff;

class OTToolsTests : public ::testing::Test {
protected:
    void SetUp() override {
		ImageDriverManager::setLogLevel("ERROR");
    }

    // Helper function to compare doubles with tolerance
    void expectNear(double actual, double expected, double tolerance = 1e-20) {
        EXPECT_NEAR(actual, expected, tolerance);
    }
};

// Test base unit - meters
TEST_F(OTToolsTests, convertToMeters_BaseUnit) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "m"), 1.0);
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(5.5, "m"), 5.5);
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(0.0, "m"), 0.0);
}

// Test metric prefixes - small units
TEST_F(OTToolsTests, convertToMeters_Micrometer) {
    // UTF-8 encoded micrometer symbol
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "\xC2\xB5m"), 1.0e-6);
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(100.0, "\xC2\xB5m"), 100.0e-6);
}

TEST_F(OTToolsTests, convertToMeters_Micrometer_ASCII) {
    // ASCII alternative
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "um"), 1.0e-6);
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(500.0, "um"), 500.0e-6);
}

TEST_F(OTToolsTests, convertToMeters_Millimeter) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "mm"), 0.001);
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(2.5, "mm"), 0.0025);
}

TEST_F(OTToolsTests, convertToMeters_Centimeter) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "cm"), 0.01);
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(10.0, "cm"), 0.1);
}

TEST_F(OTToolsTests, convertToMeters_Decimeter) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "dm"), 0.1);
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(5.0, "dm"), 0.5);
}

TEST_F(OTToolsTests, convertToMeters_Nanometer) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "nm"), 1.0e-9);
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(100.0, "nm"), 100.0e-9);
}

TEST_F(OTToolsTests, convertToMeters_Picometer) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "pm"), 1.0e-12);
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1000.0, "pm"), 1.0e-9);
}

TEST_F(OTToolsTests, convertToMeters_Femtometer) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "fm"), 1.0e-15);
}

TEST_F(OTToolsTests, convertToMeters_Attometer) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "am"), 1.0e-18);
}

TEST_F(OTToolsTests, convertToMeters_Zeptometer) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "Zm"), 1.0e-21);
}

TEST_F(OTToolsTests, convertToMeters_Yoctometer) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "Ym"), 1.0e-24);
}

TEST_F(OTToolsTests, convertToMeters_Angstrom) {
    // UTF-8 encoded Angstrom symbol
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "\xC3\x85"), 1.0e-10);
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(10.0, "\xC3\x85"), 10.0e-10);
}

// Test metric prefixes - large units
TEST_F(OTToolsTests, convertToMeters_Dekameter) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "dam"), 0.1);
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(10.0, "dam"), 1.0);
}

TEST_F(OTToolsTests, convertToMeters_Hectometer) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "hm"), 10.0);
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(5.0, "hm"), 50.0);
}

TEST_F(OTToolsTests, convertToMeters_Kilometer) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "km"), 1000.0);
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(2.5, "km"), 2500.0);
}

TEST_F(OTToolsTests, convertToMeters_Megameter) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "Mm"), 0.001);
}

TEST_F(OTToolsTests, convertToMeters_Gigameter) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "Gm"), 1.0e-9);
}

TEST_F(OTToolsTests, convertToMeters_Terameter) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "Tm"), 1.0e-12);
}

TEST_F(OTToolsTests, convertToMeters_Petameter) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "Pm"), 1.0e-15);
}

TEST_F(OTToolsTests, convertToMeters_Exameter) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "Em"), 1.0e-18);
}

// Test imperial units
TEST_F(OTToolsTests, convertToMeters_Thou) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "thou"), 0.0000254);
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1000.0, "thou"), 0.0254);
}

TEST_F(OTToolsTests, convertToMeters_Mil) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "mil"), 0.0000254);
}

TEST_F(OTToolsTests, convertToMeters_Inch) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "in"), 0.0254);
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(12.0, "in"), 0.3048);
}

TEST_F(OTToolsTests, convertToMeters_Foot) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "ft"), 0.3048);
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(3.0, "ft"), 0.9144);
}

TEST_F(OTToolsTests, convertToMeters_Yard) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "yd"), 0.9144);
}

TEST_F(OTToolsTests, convertToMeters_Mile) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "mi"), 1609.34);
}

// Test astronomical units
TEST_F(OTToolsTests, convertToMeters_AstronomicalUnit) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "ua"), 149597870700.0);
}

TEST_F(OTToolsTests, convertToMeters_LightYear) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "ly"), 9.4607e15);
}

TEST_F(OTToolsTests, convertToMeters_Parsec) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "pc"), 3.0857e16);
}

// Test typographic units
TEST_F(OTToolsTests, convertToMeters_Point) {
    expectNear(OTTools::convertToMeters(1.0, "pt"), 0.000352777778);
    expectNear(OTTools::convertToMeters(72.0, "pt"), 0.0254, 1e-6);
}

// Test pixel unit (should return value unchanged)
TEST_F(OTToolsTests, convertToMeters_Pixel) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, "pixel"), 1.0);
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(100.0, "pixel"), 100.0);
}

// Test unknown units (should return value unchanged and log warning)
TEST_F(OTToolsTests, convertToMeters_UnknownUnit) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(5.0, "unknown"), 5.0);
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(10.0, "xyz"), 10.0);
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1.0, ""), 1.0);
}

// Test edge cases
TEST_F(OTToolsTests, convertToMeters_ZeroValue) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(0.0, "m"), 0.0);
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(0.0, "km"), 0.0);
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(0.0, "um"), 0.0);
}

TEST_F(OTToolsTests, convertToMeters_NegativeValue) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(-1.0, "m"), -1.0);
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(-5.0, "km"), -5000.0);
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(-100.0, "mm"), -0.1);
}

TEST_F(OTToolsTests, convertToMeters_VeryLargeValue) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1e10, "m"), 1e10);
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1e6, "km"), 1e9);
}

TEST_F(OTToolsTests, convertToMeters_VerySmallValue) {
    EXPECT_DOUBLE_EQ(OTTools::convertToMeters(1e-10, "m"), 1e-10);
    expectNear(OTTools::convertToMeters(1e-6, "nm"), 1e-15);
}

// Test realistic microscopy use cases
TEST_F(OTToolsTests, convertToMeters_MicroscopyResolution) {
    // Typical microscopy pixel size
    expectNear(OTTools::convertToMeters(0.325, "\xC2\xB5m"), 0.325e-6);
    expectNear(OTTools::convertToMeters(0.5, "um"), 0.5e-6);
    
    // High magnification
    expectNear(OTTools::convertToMeters(100.0, "nm"), 100.0e-9);
}

// Test data type conversion
TEST_F(OTToolsTests, stringToDataType_Int8) {
    EXPECT_EQ(OTTools::stringToDataType("int8"), DataType::DT_Int8);
    EXPECT_EQ(OTTools::stringToDataType("INT8"), DataType::DT_Int8);
    EXPECT_EQ(OTTools::stringToDataType("Int8"), DataType::DT_Int8);
}

TEST_F(OTToolsTests, stringToDataType_UInt8) {
    EXPECT_EQ(OTTools::stringToDataType("uint8"), DataType::DT_Byte);
    EXPECT_EQ(OTTools::stringToDataType("UINT8"), DataType::DT_Byte);
}

TEST_F(OTToolsTests, stringToDataType_UInt16) {
    EXPECT_EQ(OTTools::stringToDataType("uint16"), DataType::DT_UInt16);
    EXPECT_EQ(OTTools::stringToDataType("UINT16"), DataType::DT_UInt16);
}

TEST_F(OTToolsTests, stringToDataType_UInt32) {
    EXPECT_EQ(OTTools::stringToDataType("uint32"), DataType::DT_UInt32);
}

TEST_F(OTToolsTests, stringToDataType_UInt64) {
    EXPECT_EQ(OTTools::stringToDataType("uint64"), DataType::DT_UInt64);
}

TEST_F(OTToolsTests, stringToDataType_Int16) {
    EXPECT_EQ(OTTools::stringToDataType("int16"), DataType::DT_Int16);
}

TEST_F(OTToolsTests, stringToDataType_Int32) {
    EXPECT_EQ(OTTools::stringToDataType("int32"), DataType::DT_Int32);
}

TEST_F(OTToolsTests, stringToDataType_Int64) {
    EXPECT_EQ(OTTools::stringToDataType("int64"), DataType::DT_Int64);
}

TEST_F(OTToolsTests, stringToDataType_Float) {
    EXPECT_EQ(OTTools::stringToDataType("float"), DataType::DT_Float32);
    EXPECT_EQ(OTTools::stringToDataType("FLOAT"), DataType::DT_Float32);
}

TEST_F(OTToolsTests, stringToDataType_Double) {
    EXPECT_EQ(OTTools::stringToDataType("double"), DataType::DT_Float64);
    EXPECT_EQ(OTTools::stringToDataType("DOUBLE"), DataType::DT_Float64);
}

TEST_F(OTToolsTests, stringToDataType_Unknown) {
    EXPECT_EQ(OTTools::stringToDataType("unknown"), DataType::DT_Unknown);
    EXPECT_EQ(OTTools::stringToDataType(""), DataType::DT_Unknown);
    EXPECT_EQ(OTTools::stringToDataType("xyz"), DataType::DT_Unknown);
}

// Test convertToSeconds function
TEST_F(OTToolsTests, convertToSeconds_BaseUnit) {
    EXPECT_DOUBLE_EQ(OTTools::convertToSeconds(1.0, "s"), 1.0);
    EXPECT_DOUBLE_EQ(OTTools::convertToSeconds(5.5, "s"), 5.5);
}

TEST_F(OTToolsTests, convertToSeconds_Millisecond) {
    EXPECT_DOUBLE_EQ(OTTools::convertToSeconds(1.0, "ms"), 0.001);
    EXPECT_DOUBLE_EQ(OTTools::convertToSeconds(1000.0, "ms"), 1.0);
}

TEST_F(OTToolsTests, convertToSeconds_Microsecond) {
    EXPECT_DOUBLE_EQ(OTTools::convertToSeconds(1.0, "us"), 1.0e-6);
}

TEST_F(OTToolsTests, convertToSeconds_Nanosecond) {
    EXPECT_DOUBLE_EQ(OTTools::convertToSeconds(1.0, "ns"), 1.0e-9);
}

TEST_F(OTToolsTests, convertToSeconds_Picosecond) {
    EXPECT_DOUBLE_EQ(OTTools::convertToSeconds(1.0, "ps"), 1.0e-12);
}

TEST_F(OTToolsTests, convertToSeconds_Femtosecond) {
    EXPECT_DOUBLE_EQ(OTTools::convertToSeconds(1.0, "fs"), 1.0e-15);
}

TEST_F(OTToolsTests, convertToSeconds_Attosecond) {
    EXPECT_DOUBLE_EQ(OTTools::convertToSeconds(1.0, "as"), 1.0e-18);
}

TEST_F(OTToolsTests, convertToSeconds_Zeptosecond) {
    EXPECT_DOUBLE_EQ(OTTools::convertToSeconds(1.0, "zs"), 1.0e-21);
}

TEST_F(OTToolsTests, convertToSeconds_Yoctosecond) {
    EXPECT_DOUBLE_EQ(OTTools::convertToSeconds(1.0, "ys"), 1.0e-24);
}

TEST_F(OTToolsTests, convertToSeconds_Minute) {
    EXPECT_DOUBLE_EQ(OTTools::convertToSeconds(1.0, "min"), 60.0);
    EXPECT_DOUBLE_EQ(OTTools::convertToSeconds(2.5, "min"), 150.0);
}

TEST_F(OTToolsTests, convertToSeconds_Hour) {
    EXPECT_DOUBLE_EQ(OTTools::convertToSeconds(1.0, "h"), 3600.0);
}

TEST_F(OTToolsTests, convertToSeconds_Day) {
    EXPECT_DOUBLE_EQ(OTTools::convertToSeconds(1.0, "d"), 86400.0);
}

TEST_F(OTToolsTests, convertToSeconds_UnknownUnit) {
    EXPECT_DOUBLE_EQ(OTTools::convertToSeconds(5.0, "unknown"), 5.0);
}

TEST_F(OTToolsTests, timeUnitToSecondsKnownUnits) {
    ASSERT_TRUE(OTTools::timeUnitToSeconds("s").has_value());
    EXPECT_DOUBLE_EQ(*OTTools::timeUnitToSeconds("s"), 1.0);
    EXPECT_DOUBLE_EQ(*OTTools::timeUnitToSeconds("ms"), 1e-3);
    EXPECT_DOUBLE_EQ(*OTTools::timeUnitToSeconds("min"), 60.0);
    EXPECT_DOUBLE_EQ(*OTTools::timeUnitToSeconds("h"), 3600.0);
}

TEST_F(OTToolsTests, timeUnitToSecondsRejectsWhatItCannotRead) {
    // A unit that is not understood must not be treated as seconds. Reporting a
    // millisecond as a second is worse than reporting no time at all.
    EXPECT_FALSE(OTTools::timeUnitToSeconds("furlong").has_value());
    EXPECT_FALSE(OTTools::timeUnitToSeconds("m").has_value());
    EXPECT_FALSE(OTTools::timeUnitToSeconds("").has_value());
}

TEST_F(OTToolsTests, parseAcquisitionDateIsUtcAndTimezoneIndependent) {
    // OME AcquisitionDate is an xsd:dateTime. Parsed as UTC so the same file
    // yields the same epoch whatever the timezone of the machine reading it.
    ASSERT_TRUE(OTTools::parseAcquisitionDate("2011-09-16T10:45:48").has_value());
    EXPECT_EQ(*OTTools::parseAcquisitionDate("2011-09-16T10:45:48"), 1316169948LL);
    EXPECT_EQ(*OTTools::parseAcquisitionDate("2011-09-16T10:45:48Z"), 1316169948LL);
    EXPECT_EQ(*OTTools::parseAcquisitionDate("2011-08-30T16:06:00"), 1314720360LL);
    // Fractional seconds are accepted and truncated.
    EXPECT_EQ(*OTTools::parseAcquisitionDate("2011-09-16T10:45:48.500"), 1316169948LL);
}

TEST_F(OTToolsTests, parseAcquisitionDateRejectsMalformedText) {
    EXPECT_FALSE(OTTools::parseAcquisitionDate("").has_value());
    EXPECT_FALSE(OTTools::parseAcquisitionDate("yesterday").has_value());
    EXPECT_FALSE(OTTools::parseAcquisitionDate("2011-09-16").has_value());
    EXPECT_FALSE(OTTools::parseAcquisitionDate("2011-13-16T10:45:48").has_value());
    EXPECT_FALSE(OTTools::parseAcquisitionDate("2011-09-32T10:45:48").has_value());
}

namespace {
    // Builds a Pixels element with one Plane per (t, c, z), in the order given.
    std::string pixelsXml(const std::string& planes) {
        return "<Pixels SizeC=\"1\" SizeZ=\"2\" SizeT=\"2\">" + planes + "</Pixels>";
    }
}

TEST_F(OTToolsTests, planeTimestampsAreReadFromDeltaT) {
    tinyxml2::XMLDocument doc;
    ASSERT_EQ(doc.Parse(pixelsXml(
        "<Plane TheT=\"0\" TheC=\"0\" TheZ=\"0\" DeltaT=\"0.0\"/>"
        "<Plane TheT=\"0\" TheC=\"0\" TheZ=\"1\" DeltaT=\"0.5\"/>"
        "<Plane TheT=\"1\" TheC=\"0\" TheZ=\"0\" DeltaT=\"1.0\"/>"
        "<Plane TheT=\"1\" TheC=\"0\" TheZ=\"1\" DeltaT=\"1.5\"/>").c_str()),
        tinyxml2::XML_SUCCESS);
    const auto times = OTTools::collectPlaneTimestamps(doc.RootElement(), 2, 1, 2);
    ASSERT_TRUE(times.has_value());
    ASSERT_EQ(times->size(), 4u);
    // index = (t * numZ + z) * numC + c
    EXPECT_DOUBLE_EQ((*times)[0], 0.0);
    EXPECT_DOUBLE_EQ((*times)[1], 0.5);
    EXPECT_DOUBLE_EQ((*times)[2], 1.0);
    EXPECT_DOUBLE_EQ((*times)[3], 1.5);
}

TEST_F(OTToolsTests, planeTimestampsUseTheStatedUnit) {
    tinyxml2::XMLDocument doc;
    ASSERT_EQ(doc.Parse(pixelsXml(
        "<Plane TheT=\"0\" TheC=\"0\" TheZ=\"0\" DeltaT=\"0\" DeltaTUnit=\"ms\"/>"
        "<Plane TheT=\"0\" TheC=\"0\" TheZ=\"1\" DeltaT=\"500\" DeltaTUnit=\"ms\"/>"
        "<Plane TheT=\"1\" TheC=\"0\" TheZ=\"0\" DeltaT=\"1000\" DeltaTUnit=\"ms\"/>"
        "<Plane TheT=\"1\" TheC=\"0\" TheZ=\"1\" DeltaT=\"1500\" DeltaTUnit=\"ms\"/>").c_str()),
        tinyxml2::XML_SUCCESS);
    const auto times = OTTools::collectPlaneTimestamps(doc.RootElement(), 2, 1, 2);
    ASSERT_TRUE(times.has_value());
    EXPECT_DOUBLE_EQ((*times)[1], 0.5);
    EXPECT_DOUBLE_EQ((*times)[3], 1.5);
}

TEST_F(OTToolsTests, aPlaneWithoutADeltaTDiscardsTheWholeSeries) {
    // The getter is addressed by plane, so a missing entry would leave one plane
    // reporting a time it does not have. No timestamps beats wrong ones.
    tinyxml2::XMLDocument doc;
    ASSERT_EQ(doc.Parse(pixelsXml(
        "<Plane TheT=\"0\" TheC=\"0\" TheZ=\"0\" DeltaT=\"0.0\"/>"
        "<Plane TheT=\"0\" TheC=\"0\" TheZ=\"1\" DeltaT=\"0.5\"/>"
        "<Plane TheT=\"1\" TheC=\"0\" TheZ=\"0\"/>"
        "<Plane TheT=\"1\" TheC=\"0\" TheZ=\"1\" DeltaT=\"1.5\"/>").c_str()),
        tinyxml2::XML_SUCCESS);
    EXPECT_FALSE(OTTools::collectPlaneTimestamps(doc.RootElement(), 2, 1, 2).has_value());
}

TEST_F(OTToolsTests, anUnreadableDeltaTUnitDiscardsTheWholeSeries) {
    tinyxml2::XMLDocument doc;
    ASSERT_EQ(doc.Parse(pixelsXml(
        "<Plane TheT=\"0\" TheC=\"0\" TheZ=\"0\" DeltaT=\"0\" DeltaTUnit=\"furlong\"/>"
        "<Plane TheT=\"0\" TheC=\"0\" TheZ=\"1\" DeltaT=\"1\" DeltaTUnit=\"furlong\"/>"
        "<Plane TheT=\"1\" TheC=\"0\" TheZ=\"0\" DeltaT=\"2\" DeltaTUnit=\"furlong\"/>"
        "<Plane TheT=\"1\" TheC=\"0\" TheZ=\"1\" DeltaT=\"3\" DeltaTUnit=\"furlong\"/>").c_str()),
        tinyxml2::XML_SUCCESS);
    EXPECT_FALSE(OTTools::collectPlaneTimestamps(doc.RootElement(), 2, 1, 2).has_value());
}

TEST_F(OTToolsTests, aPlaneNamingACoordinateOutsideTheSceneDiscardsTheSeries) {
    tinyxml2::XMLDocument doc;
    ASSERT_EQ(doc.Parse(pixelsXml(
        "<Plane TheT=\"0\" TheC=\"0\" TheZ=\"0\" DeltaT=\"0.0\"/>"
        "<Plane TheT=\"0\" TheC=\"0\" TheZ=\"1\" DeltaT=\"0.5\"/>"
        "<Plane TheT=\"1\" TheC=\"0\" TheZ=\"0\" DeltaT=\"1.0\"/>"
        "<Plane TheT=\"9\" TheC=\"0\" TheZ=\"1\" DeltaT=\"1.5\"/>").c_str()),
        tinyxml2::XML_SUCCESS);
    EXPECT_FALSE(OTTools::collectPlaneTimestamps(doc.RootElement(), 2, 1, 2).has_value());
}

TEST_F(OTToolsTests, noPlaneElementsMeansNoTimestampsRatherThanAnError) {
    tinyxml2::XMLDocument doc;
    ASSERT_EQ(doc.Parse(pixelsXml("").c_str()), tinyxml2::XML_SUCCESS);
    EXPECT_FALSE(OTTools::collectPlaneTimestamps(doc.RootElement(), 2, 1, 2).has_value());
}

TEST_F(OTToolsTests, planesMissingAltogetherDiscardTheWholeSeries) {
    // Two of the four planes are simply not described. Filling the gaps with 0
    // would have two planes reporting a time that belongs to neither.
    tinyxml2::XMLDocument doc;
    ASSERT_EQ(doc.Parse(pixelsXml(
        "<Plane TheT=\"0\" TheC=\"0\" TheZ=\"0\" DeltaT=\"0.0\"/>"
        "<Plane TheT=\"1\" TheC=\"0\" TheZ=\"1\" DeltaT=\"1.5\"/>").c_str()),
        tinyxml2::XML_SUCCESS);
    EXPECT_FALSE(OTTools::collectPlaneTimestamps(doc.RootElement(), 2, 1, 2).has_value());
}

TEST_F(OTToolsTests, aPlaneDescribedTwiceDoesNotCompleteTheSeries) {
    // The same plane twice is still two planes short, not a full series.
    tinyxml2::XMLDocument doc;
    ASSERT_EQ(doc.Parse(pixelsXml(
        "<Plane TheT=\"0\" TheC=\"0\" TheZ=\"0\" DeltaT=\"0.0\"/>"
        "<Plane TheT=\"0\" TheC=\"0\" TheZ=\"0\" DeltaT=\"0.1\"/>"
        "<Plane TheT=\"0\" TheC=\"0\" TheZ=\"1\" DeltaT=\"0.5\"/>").c_str()),
        tinyxml2::XML_SUCCESS);
    EXPECT_FALSE(OTTools::collectPlaneTimestamps(doc.RootElement(), 2, 1, 2).has_value());
}

TEST_F(OTToolsTests, tFrameResolutionUsesTheTimeUnitNotTheZUnit) {
    // The two unit attributes are deliberately different here: reading
    // PhysicalSizeZUnit for the T resolution scales a time by a length unit.
    tinyxml2::XMLDocument doc;
    ASSERT_EQ(doc.Parse("<Pixels PhysicalSizeT=\"500\" PhysicalSizeTUnit=\"ms\""
                        " PhysicalSizeZ=\"2\" PhysicalSizeZUnit=\"um\"/>"),
              tinyxml2::XML_SUCCESS);
    EXPECT_DOUBLE_EQ(OTTools::readTFrameResolution(doc.RootElement()), 0.5);
}

TEST_F(OTToolsTests, tFrameResolutionDefaultsToSeconds) {
    tinyxml2::XMLDocument doc;
    ASSERT_EQ(doc.Parse("<Pixels PhysicalSizeT=\"2.5\"/>"), tinyxml2::XML_SUCCESS);
    EXPECT_DOUBLE_EQ(OTTools::readTFrameResolution(doc.RootElement()), 2.5);
}

TEST_F(OTToolsTests, tFrameResolutionIsZeroWhenItsUnitCannotBeRead) {
    tinyxml2::XMLDocument doc;
    ASSERT_EQ(doc.Parse("<Pixels PhysicalSizeT=\"500\" PhysicalSizeTUnit=\"furlong\"/>"),
              tinyxml2::XML_SUCCESS);
    EXPECT_DOUBLE_EQ(OTTools::readTFrameResolution(doc.RootElement()), 0.);
}

TEST_F(OTToolsTests, zSliceResolutionUsesItsOwnLengthUnit) {
    tinyxml2::XMLDocument doc;
    ASSERT_EQ(doc.Parse("<Pixels PhysicalSizeZ=\"2\" PhysicalSizeZUnit=\"um\""
                        " PhysicalSizeT=\"500\" PhysicalSizeTUnit=\"ms\"/>"),
              tinyxml2::XML_SUCCESS);
    EXPECT_DOUBLE_EQ(OTTools::readZSliceResolution(doc.RootElement()), 2e-6);
}

TEST_F(OTToolsTests, tFrameResolutionReadsTimeIncrement) {
    // TimeIncrement is the OME-XML attribute for the interval between time
    // points. PhysicalSizeT is not in the schema at all.
    tinyxml2::XMLDocument doc;
    ASSERT_EQ(doc.Parse("<Pixels TimeIncrement=\"500\" TimeIncrementUnit=\"ms\"/>"),
              tinyxml2::XML_SUCCESS);
    EXPECT_DOUBLE_EQ(OTTools::readTFrameResolution(doc.RootElement()), 0.5);
}

TEST_F(OTToolsTests, tFrameResolutionFallsBackToPhysicalSizeT) {
    // slideio's own converter writes PhysicalSizeT, so files it produced are
    // still read back. TimeIncrement wins when a file states both.
    tinyxml2::XMLDocument ours;
    ASSERT_EQ(ours.Parse("<Pixels PhysicalSizeT=\"2.5\"/>"), tinyxml2::XML_SUCCESS);
    EXPECT_DOUBLE_EQ(OTTools::readTFrameResolution(ours.RootElement()), 2.5);

    tinyxml2::XMLDocument both;
    ASSERT_EQ(both.Parse("<Pixels TimeIncrement=\"7\" PhysicalSizeT=\"2.5\"/>"),
              tinyxml2::XML_SUCCESS);
    EXPECT_DOUBLE_EQ(OTTools::readTFrameResolution(both.RootElement()), 7.0);
}

TEST_F(OTToolsTests, zSliceResolutionDefaultsToMicrometres) {
    // PhysicalSizeZUnit carries a schema default of um; treating an absent unit
    // as metres is wrong by a factor of a million.
    tinyxml2::XMLDocument doc;
    ASSERT_EQ(doc.Parse("<Pixels PhysicalSizeZ=\"0.5\"/>"), tinyxml2::XML_SUCCESS);
    EXPECT_DOUBLE_EQ(OTTools::readZSliceResolution(doc.RootElement()), 5e-7);
}

TEST_F(OTToolsTests, parseAcquisitionDateRejectsNegativeFields) {
    // %2d will consume a sign, so the guard needs a floor as well as a ceiling.
    EXPECT_FALSE(OTTools::parseAcquisitionDate("2011-09-16T-1:45:48").has_value());
    EXPECT_FALSE(OTTools::parseAcquisitionDate("2011-09-16T10:-5:48").has_value());
    EXPECT_FALSE(OTTools::parseAcquisitionDate("2011-09-16T10:45:-8").has_value());
    EXPECT_FALSE(OTTools::parseAcquisitionDate("-011-09-16T10:45:48").has_value());
}

TEST_F(OTToolsTests, parseAcquisitionDateRejectsAnOffsetItCannotRead) {
    // "+0200" without the colon is not xsd:dateTime. Silently reading it as UTC
    // would be a two hour error in something whose point is one file, one epoch.
    EXPECT_FALSE(OTTools::parseAcquisitionDate("2011-09-16T10:45:48+0200").has_value());
    EXPECT_FALSE(OTTools::parseAcquisitionDate("2011-09-16T10:45:48+").has_value());
    // A well formed offset is still applied.
    ASSERT_TRUE(OTTools::parseAcquisitionDate("2011-09-16T12:45:48+02:00").has_value());
    EXPECT_EQ(*OTTools::parseAcquisitionDate("2011-09-16T12:45:48+02:00"), 1316169948LL);
    EXPECT_EQ(*OTTools::parseAcquisitionDate("2011-09-16T08:45:48-02:00"), 1316169948LL);
}

TEST_F(OTToolsTests, aDuplicatePlaneIsRejectedEvenWhenTheSeriesIsOtherwiseComplete) {
    // All four planes are described, and one of them twice. Counting distinct
    // indices alone would accept this and let the second DeltaT win silently.
    tinyxml2::XMLDocument doc;
    ASSERT_EQ(doc.Parse(pixelsXml(
        "<Plane TheT=\"0\" TheC=\"0\" TheZ=\"0\" DeltaT=\"0.0\"/>"
        "<Plane TheT=\"0\" TheC=\"0\" TheZ=\"1\" DeltaT=\"0.5\"/>"
        "<Plane TheT=\"1\" TheC=\"0\" TheZ=\"0\" DeltaT=\"1.0\"/>"
        "<Plane TheT=\"1\" TheC=\"0\" TheZ=\"1\" DeltaT=\"1.5\"/>"
        "<Plane TheT=\"0\" TheC=\"0\" TheZ=\"0\" DeltaT=\"9.9\"/>").c_str()),
        tinyxml2::XML_SUCCESS);
    EXPECT_FALSE(OTTools::collectPlaneTimestamps(doc.RootElement(), 2, 1, 2).has_value());
}
