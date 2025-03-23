#include <gtest/gtest.h>
#include "slideio/core/tools/endian.hpp"

class EndianTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
    }
    static void TearDownTestSuite() {
    }
};

using namespace slideio;

TEST_F(EndianTest, swapBytes16) {
    uint16_t value = 0x1234;
    uint16_t expected = 0x3412;
    EXPECT_EQ(Endian::swapBytes(value), expected);
}

TEST_F(EndianTest, swapBytes32) {
    uint32_t value = 0x12345678;
    uint32_t expected = 0x78563412;
    EXPECT_EQ(Endian::swapBytes(value), expected);
}

TEST_F(EndianTest, swapBytes64) {
    uint64_t value = 0x123456789ABCDEF0;
    uint64_t expected = 0xF0DEBC9A78563412;
    EXPECT_EQ(Endian::swapBytes(value), expected);
}

TEST_F(EndianTest, fromLittleEndianToNative_16Bit_LittleEndian) {
    uint16_t value = 0x1234;
    uint16_t expected = 0x1234;
    if (Endian::isLittleEndian()) {
        EXPECT_EQ(Endian::fromLittleEndianToNative(value), expected);
    }
    else {
        EXPECT_EQ(Endian::fromLittleEndianToNative(value), Endian::swapBytes(expected));
    }
}

TEST_F(EndianTest, fromLittleEndianToNative_16Bit_BigEndian) {
    uint16_t value = 0x1234;
    uint16_t expected = 0x3412;
    if (!Endian::isLittleEndian()) {
        EXPECT_EQ(Endian::fromLittleEndianToNative(value), expected);
    }
    else {
        EXPECT_EQ(Endian::fromLittleEndianToNative(value), Endian::swapBytes(expected));
    }
}

TEST_F(EndianTest, fromLittleEndianToNative_32Bit_LittleEndian) {
    uint32_t value = 0x12345678;
    uint32_t expected = 0x12345678;
    if (Endian::isLittleEndian()) {
        EXPECT_EQ(Endian::fromLittleEndianToNative(value), expected);
    }
    else {
        EXPECT_EQ(Endian::fromLittleEndianToNative(value), Endian::swapBytes(expected));
    }
}

TEST_F(EndianTest, fromLittleEndianToNative_32Bit_BigEndian) {
    uint32_t value = 0x12345678;
    uint32_t expected = 0x78563412;
    if (!Endian::isLittleEndian()) {
        EXPECT_EQ(Endian::fromLittleEndianToNative(value), expected);
    }
    else {
        EXPECT_EQ(Endian::fromLittleEndianToNative(value), Endian::swapBytes(expected));
    }
}

TEST_F(EndianTest, fromLittleEndianToNative_64Bit_LittleEndian) {
    uint64_t value = 0x123456789ABCDEF0;
    uint64_t expected = 0x123456789ABCDEF0;
    if (Endian::isLittleEndian()) {
        EXPECT_EQ(Endian::fromLittleEndianToNative(value), expected);
    }
    else {
        EXPECT_EQ(Endian::fromLittleEndianToNative(value), Endian::swapBytes(expected));
    }
}

TEST_F(EndianTest, fromLittleEndianToNative_64Bit_BigEndian) {
    uint64_t value = 0x123456789ABCDEF0;
    uint64_t expected = 0xF0DEBC9A78563412;
    if (!Endian::isLittleEndian()) {
        EXPECT_EQ(Endian::fromLittleEndianToNative(value), expected);
    }
    else {
        EXPECT_EQ(Endian::fromLittleEndianToNative(value), Endian::swapBytes(expected));
    }
}

TEST_F(EndianTest, isLittleEndian) {
    uint16_t number = 0x1;
    char* numPtr = (char*)&number;
    bool expected = (numPtr[0] == 1);
    EXPECT_EQ(Endian::isLittleEndian(), expected);
}

TEST_F(EndianTest, fromLittleEndianToNative) {
    {
        uint16_t value = 0x1234;
        uint16_t expected = Endian::isLittleEndian() ? 0x1234 : 0x3412;
        EXPECT_EQ(Endian::fromLittleEndianToNative(value), expected);
    }
    {
        uint32_t value = 0x12345678;
        uint32_t expected = Endian::isLittleEndian() ? 0x12345678 : 0x78563412;
        EXPECT_EQ(Endian::fromLittleEndianToNative(value), expected);
    }
    {
        uint64_t value = 0x123456789ABCDEF0;
        uint64_t expected = Endian::isLittleEndian() ? 0x123456789ABCDEF0 : 0xF0DEBC9A78563412;
        EXPECT_EQ(Endian::fromLittleEndianToNative(value), expected);
    }
    {
        float value = 1234.5678f;
        float expected = Endian::isLittleEndian() ? 1234.5678f : Endian::swapBytes(value);
        EXPECT_EQ(Endian::fromLittleEndianToNative(value), expected);
    }
    {
        double value = 12345678.12345678;
        double expected = Endian::isLittleEndian() ? 12345678.12345678 : Endian::swapBytes(value);
        EXPECT_EQ(Endian::fromLittleEndianToNative(value), expected);
    }
}

TEST_F(EndianTest, FromLittleEndianToNative_DataType_Byte) {
    {
        uint8_t data[] = { 0x12, 0x34 };
        uint8_t expected[] = { 0x12, 0x34 }; // No change expected for byte data
        Endian::fromLittleEndianToNative(DataType::DT_Byte, data, sizeof(data));
        EXPECT_EQ(data[0], expected[0]);
        EXPECT_EQ(data[1], expected[1]);
    }
    {
        int16_t data[] = { 0x1234, 0x5678 };
        int16_t val1 = Endian::isLittleEndian() ? 0x1234 : 0x3412;
        int16_t val2 = Endian::isLittleEndian() ? 0x5678 : 0x7856;
        int16_t expected[] = { val1, val2 };
        Endian::fromLittleEndianToNative(DataType::DT_Int16, data, sizeof(data));
        EXPECT_EQ(data[0], expected[0]);
        EXPECT_EQ(data[1], expected[1]);
    }
    {
        int32_t data1 = 0x12345678;
        int32_t data2 = 0x9ABCDEF0;
        int32_t data[] = { data1, data2 };
        int32_t expected1 = Endian::isLittleEndian() ? 0x12345678 : 0x78563412;
        int32_t expected2 = Endian::isLittleEndian() ? 0x9ABCDEF0 : 0xF0DEBC9A;
        int32_t expected[] = { expected1, expected2 };
        Endian::fromLittleEndianToNative(DataType::DT_Int32, data, sizeof(data));
        EXPECT_EQ(data[0], expected[0]);
        EXPECT_EQ(data[1], expected[1]);
    }
    {
        float data[] = { 1234.5678f, 8765.4321f };
        float expected[] = { Endian::isLittleEndian() ? 1234.5678f : Endian::swapBytes(data[0]), Endian::isLittleEndian() ? 8765.4321f : Endian::swapBytes(data[1]) };
        Endian::fromLittleEndianToNative(DataType::DT_Float32, data, sizeof(data));
        EXPECT_EQ(data[0], expected[0]);
        EXPECT_EQ(data[1], expected[1]);
    }
    {
        double data[] = { 12345678.12345678, 87654321.87654321 };
        double expected[] = { Endian::isLittleEndian() ? 12345678.12345678 : Endian::swapBytes(data[0]), Endian::isLittleEndian() ? 87654321.87654321 : Endian::swapBytes(data[1]) };
        Endian::fromLittleEndianToNative(DataType::DT_Float64, data, sizeof(data));
        EXPECT_EQ(data[0], expected[0]);
        EXPECT_EQ(data[1], expected[1]);
    }
    {
        uint16_t data[] = { 0x1234, 0x5678 };
        uint16_t expected1 = Endian::isLittleEndian() ? 0x1234 : 0x3412;
        uint16_t expected2 = Endian::isLittleEndian() ? 0x5678 : 0x7856;
        uint16_t expected[] = { expected1, expected2 };
        Endian::fromLittleEndianToNative(DataType::DT_UInt16, data, sizeof(data));
        EXPECT_EQ(data[0], expected[0]);
        EXPECT_EQ(data[1], expected[1]);
    }
    {
        uint32_t data[] = { 0x12345678, 0x9ABCDEF0 };
        uint32_t expected1 = Endian::isLittleEndian() ? 0x12345678 : 0x78563412;
        uint32_t expected2 = Endian::isLittleEndian() ? 0x9ABCDEF0 : 0xF0DEBC9A;
        uint32_t expected[] = { expected1, expected2 };
        Endian::fromLittleEndianToNative(DataType::DT_UInt32, data, sizeof(data));
        EXPECT_EQ(data[0], expected[0]);
        EXPECT_EQ(data[1], expected[1]);
    }
    {
        int64_t data[] = { 0x123456789ABCDEF0, 0x0FEDCBA987654321 };
        int64_t expected1 = Endian::isLittleEndian() ? 0x123456789ABCDEF0 : 0xF0DEBC9A78563412;
        int64_t expected2 = Endian::isLittleEndian() ? 0x0FEDCBA987654321 : 0x21436587A9CBED0F;
        int64_t expected[] = { expected1, expected2 };
        Endian::fromLittleEndianToNative(DataType::DT_Int64, data, sizeof(data));
        EXPECT_EQ(data[0], expected[0]);
        EXPECT_EQ(data[1], expected[1]);
    }
    {
        uint64_t data[] = { 0x123456789ABCDEF0, 0x0FEDCBA987654321 };
        uint64_t expected1 = Endian::isLittleEndian() ? 0x123456789ABCDEF0 : 0xF0DEBC9A78563412;
        uint64_t expected2 = Endian::isLittleEndian() ? 0x0FEDCBA987654321 : 0x21436587A9CBED0F;
        uint64_t expected[] = { expected1, expected2 };
        Endian::fromLittleEndianToNative(DataType::DT_UInt64, data, sizeof(data));
        EXPECT_EQ(data[0], expected[0]);
        EXPECT_EQ(data[1], expected[1]);
    }
}