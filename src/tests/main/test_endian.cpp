#include <gtest/gtest.h>
#include "slideio/core/tools/endian.hpp"

class EndianTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
    }
    static void TearDownTestSuite() {
    }
};


TEST_F(EndianTest, swapBytes16) {
    uint16_t value = 0x1234;
    uint16_t expected = 0x3412;
    EXPECT_EQ(slideio::Endian::swapBytes(value), expected);
}

TEST_F(EndianTest, swapBytes32) {
    uint32_t value = 0x12345678;
    uint32_t expected = 0x78563412;
    EXPECT_EQ(slideio::Endian::swapBytes(value), expected);
}

TEST_F(EndianTest, swapBytes64) {
    uint64_t value = 0x123456789ABCDEF0;
    uint64_t expected = 0xF0DEBC9A78563412;
    EXPECT_EQ(slideio::Endian::swapBytes(value), expected);
}

TEST_F(EndianTest, fromLittleEndianToNative_16Bit_LittleEndian) {
    uint16_t value = 0x1234;
    uint16_t expected = 0x1234;
    if (slideio::Endian::isLittleEndian()) {
        EXPECT_EQ(slideio::Endian::fromLittleEndianToNative(value), expected);
    }
    else {
        EXPECT_EQ(slideio::Endian::fromLittleEndianToNative(value), slideio::Endian::swapBytes(expected));
    }
}

TEST_F(EndianTest, fromLittleEndianToNative_16Bit_BigEndian) {
    uint16_t value = 0x1234;
    uint16_t expected = 0x3412;
    if (!slideio::Endian::isLittleEndian()) {
        EXPECT_EQ(slideio::Endian::fromLittleEndianToNative(value), expected);
    }
    else {
        EXPECT_EQ(slideio::Endian::fromLittleEndianToNative(value), slideio::Endian::swapBytes(expected));
    }
}

TEST_F(EndianTest, fromLittleEndianToNative_32Bit_LittleEndian) {
    uint32_t value = 0x12345678;
    uint32_t expected = 0x12345678;
    if (slideio::Endian::isLittleEndian()) {
        EXPECT_EQ(slideio::Endian::fromLittleEndianToNative(value), expected);
    }
    else {
        EXPECT_EQ(slideio::Endian::fromLittleEndianToNative(value), slideio::Endian::swapBytes(expected));
    }
}

TEST_F(EndianTest, fromLittleEndianToNative_32Bit_BigEndian) {
    uint32_t value = 0x12345678;
    uint32_t expected = 0x78563412;
    if (!slideio::Endian::isLittleEndian()) {
        EXPECT_EQ(slideio::Endian::fromLittleEndianToNative(value), expected);
    }
    else {
        EXPECT_EQ(slideio::Endian::fromLittleEndianToNative(value), slideio::Endian::swapBytes(expected));
    }
}

TEST_F(EndianTest, fromLittleEndianToNative_64Bit_LittleEndian) {
    uint64_t value = 0x123456789ABCDEF0;
    uint64_t expected = 0x123456789ABCDEF0;
    if (slideio::Endian::isLittleEndian()) {
        EXPECT_EQ(slideio::Endian::fromLittleEndianToNative(value), expected);
    }
    else {
        EXPECT_EQ(slideio::Endian::fromLittleEndianToNative(value), slideio::Endian::swapBytes(expected));
    }
}

TEST_F(EndianTest, fromLittleEndianToNative_64Bit_BigEndian) {
    uint64_t value = 0x123456789ABCDEF0;
    uint64_t expected = 0xF0DEBC9A78563412;
    if (!slideio::Endian::isLittleEndian()) {
        EXPECT_EQ(slideio::Endian::fromLittleEndianToNative(value), expected);
    }
    else {
        EXPECT_EQ(slideio::Endian::fromLittleEndianToNative(value), slideio::Endian::swapBytes(expected));
    }
}

TEST_F(EndianTest, isLittleEndian) {
    uint16_t number = 0x1;
    char* numPtr = (char*)&number;
    bool expected = (numPtr[0] == 1);
    EXPECT_EQ(slideio::Endian::isLittleEndian(), expected);
}
