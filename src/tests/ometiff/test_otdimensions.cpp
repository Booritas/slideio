#include <gtest/gtest.h>
#include "slideio/drivers/ome-tiff/otdimensions.hpp"

using namespace slideio;
using namespace slideio::ometiff;

class OTDimensionsTest : public ::testing::Test {
protected:

    void SetUp() override {
    }
};

TEST_F(OTDimensionsTest, InitTest) {
    OTDimensions dimensions;
    dimensions.init("XYZCT", 3, 5, 7, 1);
	EXPECT_EQ(dimensions.getNumDimensions(), 3);
	std::array<std::string, 3> expectedLabels = { DimZ, DimC, DimT };
    EXPECT_EQ(dimensions.getOrder(), expectedLabels);
	std::array<int, 3> expectedSizes = { 5, 3, 7 };
    EXPECT_EQ(dimensions.getSizes(), expectedSizes);
}

TEST_F(OTDimensionsTest, GetDimensionSizeTest) {
    OTDimensions dimensions;
    dimensions.init("XYZCT", 3, 5, 7, 1);
    EXPECT_EQ(dimensions.getDimensionSize(DimC), 3);
    EXPECT_EQ(dimensions.getDimensionSize(DimZ), 5);
    EXPECT_EQ(dimensions.getDimensionSize(DimT), 7);
}

TEST_F(OTDimensionsTest, GetDimensionIndexTest) {
    OTDimensions dimensions;
    dimensions.init("XYZCT", 3, 5, 7, 1);
    EXPECT_EQ(dimensions.getDimensionIndex(DimZ), 0);
    EXPECT_EQ(dimensions.getDimensionIndex(DimC), 1);
    EXPECT_EQ(dimensions.getDimensionIndex(DimT), 2);
}

TEST_F(OTDimensionsTest, IncrementCoordinatesTest) {
    OTDimensions dimensions;
    dimensions.init("XYZCT", 1, 2, 3, 1);

    std::array<int,3> coordinates = { 0, 0, 0 };
    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));

	std::array<int, 3> val1 = { 1, 0, 0 };
    EXPECT_EQ(coordinates, val1);
    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));

	std::array<int, 3> val2 = { 0, 0, 1 };
    EXPECT_EQ(coordinates, val2);
    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));

	std::array<int, 3> val3 = { 1, 0, 1 };
    EXPECT_EQ(coordinates, val3);

    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
	std::array<int, 3> val4 = { 0, 0, 2 };
    EXPECT_EQ(coordinates, val4);

    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
	std::array<int, 3> val5 = { 1, 0, 2 };
    EXPECT_EQ(coordinates, val5);
}

TEST_F(OTDimensionsTest, IncrementCoordinatesTestMultichannel) {
    OTDimensions dimensions;
    dimensions.init("XYTCZ", 6, 3, 2, 3);
    std::array<int,3> coordinates = { 0, 0, 0 };
    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
	std::array<int,3> val1 = { 1, 0, 0 };
    EXPECT_EQ(coordinates, val1);
    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
	std::array<int, 3> val2 = { 0, 3, 0 };
    EXPECT_EQ(coordinates, val2);
    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
	std::array<int, 3> val3 = { 1, 3, 0 };
    EXPECT_EQ(coordinates, val3);


    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
	std::array<int, 3> val4 = { 0, 0, 1 };
    EXPECT_EQ(coordinates, val4);
    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
	std::array<int, 3> val5 = { 1, 0, 1 };
    EXPECT_EQ(coordinates, val5);
    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
	std::array<int, 3> val6 = { 0, 3, 1 };
    EXPECT_EQ(coordinates, val6);
    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
	std::array<int, 3> val7 = { 1, 3, 1 };
    EXPECT_EQ(coordinates, val7);

    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
	std::array<int, 3> val8 = { 0, 0, 2 };
    EXPECT_EQ(coordinates, val8);
    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
	std::array<int, 3> val9 = { 1, 0, 2 };
    EXPECT_EQ(coordinates, val9);
    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
	std::array<int, 3> val10 = { 0, 3, 2 };
    EXPECT_EQ(coordinates, val10);
    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
	std::array<int, 3> val11 = { 1, 3, 2 };
    EXPECT_EQ(coordinates, val11);

    EXPECT_FALSE(dimensions.incrementCoordinates(coordinates));
}

TEST_F(OTDimensionsTest, CreateCoordinatesTest) {
    OTDimensions dimensions;
    dimensions.init("XYZCT", 3, 5, 7, 1);
    std::list<std::pair<std::string, int>> coordList1 = { {DimZ, 3}, {DimC, 1}, {DimT, 4} };
    std::list<std::pair<std::string, int>> coordList2 = { {DimC, 1}, {DimT, 4}, {DimZ, 3} };
    std::list<std::pair<std::string, int>> coordList3 = { {DimT, 4},{DimC, 1},  {DimZ, 3} };
	std::array<int, 3> expectedCoords = { 3, 1, 4 };
    EXPECT_EQ(dimensions.createCoordinates(coordList1), expectedCoords);
    EXPECT_EQ(dimensions.createCoordinates(coordList2), expectedCoords);
    EXPECT_EQ(dimensions.createCoordinates(coordList3), expectedCoords);
}