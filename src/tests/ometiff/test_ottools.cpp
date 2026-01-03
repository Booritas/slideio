#include <gtest/gtest.h>
#include "slideio/drivers/ome-tiff/ottools.hpp"
#include <cmath>

using namespace slideio;
using namespace slideio::ometiff;

class OTToolsTests : public ::testing::Test {
protected:
    void SetUp() override {
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
