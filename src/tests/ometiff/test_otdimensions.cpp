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
    EXPECT_EQ(dimensions.getOrder(), std::vector<std::string>({ DimZ, DimC, DimT }));
    EXPECT_EQ(dimensions.getSizes(), std::vector<int>({ 5, 3, 7 }));
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
    std::vector<int> coordinates = { 0, 0, 0 };
    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
    EXPECT_EQ(coordinates, std::vector<int>({ 1, 0, 0 }));

    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
    EXPECT_EQ(coordinates, std::vector<int>({ 0, 0, 1 }));

    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
    EXPECT_EQ(coordinates, std::vector<int>({ 1, 0, 1 }));

    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
    EXPECT_EQ(coordinates, std::vector<int>({ 0, 0, 2 }));

    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
    EXPECT_EQ(coordinates, std::vector<int>({ 1, 0, 2 }));
}

TEST_F(OTDimensionsTest, IncrementCoordinatesTestMultichannel) {
    OTDimensions dimensions;
    dimensions.init("XYTCZ", 6, 3, 2, 3);
    std::vector<int> coordinates = { 0, 0, 0 };
    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
    EXPECT_EQ(coordinates, std::vector<int>({ 1, 0, 0 }));
    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
    EXPECT_EQ(coordinates, std::vector<int>({ 0, 3, 0 }));
    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
    EXPECT_EQ(coordinates, std::vector<int>({ 1, 3, 0 }));


    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
    EXPECT_EQ(coordinates, std::vector<int>({ 0, 0, 1 }));
    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
    EXPECT_EQ(coordinates, std::vector<int>({ 1, 0, 1 }));
    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
    EXPECT_EQ(coordinates, std::vector<int>({ 0, 3, 1 }));
    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
    EXPECT_EQ(coordinates, std::vector<int>({ 1, 3, 1 }));

    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
    EXPECT_EQ(coordinates, std::vector<int>({ 0, 0, 2 }));
    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
    EXPECT_EQ(coordinates, std::vector<int>({ 1, 0, 2 }));
    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
    EXPECT_EQ(coordinates, std::vector<int>({ 0, 3, 2 }));
    EXPECT_TRUE(dimensions.incrementCoordinates(coordinates));
    EXPECT_EQ(coordinates, std::vector<int>({ 1, 3, 2 }));

    EXPECT_FALSE(dimensions.incrementCoordinates(coordinates));
}

TEST_F(OTDimensionsTest, CreateCoordinatesTest) {
    OTDimensions dimensions;
    dimensions.init("XYZCT", 3, 5, 7, 1);
    std::list<std::pair<std::string, int>> coordList1 = { {DimZ, 3}, {DimC, 1}, {DimT, 4} };
    std::list<std::pair<std::string, int>> coordList2 = { {DimC, 1}, {DimT, 4}, {DimZ, 3} };
    std::list<std::pair<std::string, int>> coordList3 = { {DimT, 4},{DimC, 1},  {DimZ, 3} };
    EXPECT_EQ(dimensions.createCoordinates(coordList1), std::vector<int>({ 3, 1, 4 }));
    EXPECT_EQ(dimensions.createCoordinates(coordList2), std::vector<int>({ 3, 1, 4 }));
    EXPECT_EQ(dimensions.createCoordinates(coordList3), std::vector<int>({ 3, 1, 4 }));
}