// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/dimensions.hpp"
#include "slideio/base/exceptions.hpp"
#include <gtest/gtest.h>
#include <string>
#include <list>
#include <array>


using namespace slideio;

TEST(DimensionsTest, Constructor) {
    std::array<std::string,3> labels = { "X", "Y", "Z" };
    std::array<int,3> sizes = { 10, 20, 30 };
	std::array<int,3> increments = { 1, 1, 1 };
    Dimensions<3> dims(labels, sizes, increments);

    EXPECT_EQ(dims.getOrder(), labels);
    EXPECT_EQ(dims.getSizes(), sizes);
	for (int index = 0; index < labels.size(); ++index) {
		EXPECT_EQ(dims.getDimensionSize(labels[index]), sizes[index]);
		EXPECT_EQ(dims.getDimensionIndex(labels[index]), index);
	}
}

TEST(DimensionsTest, IncrementCoordinates) {
    std::array<std::string,2> labels = { "X", "Y" };
    std::array<int,2> sizes = { 2, 3 };
	std::array<int,2> increments = { 1, 1 };
    Dimensions<2> dims(labels, sizes, increments);

    std::array<int,2> coordinates = { 0, 0 };
    EXPECT_TRUE(dims.incrementCoordinates(coordinates));
    const std::array<int, 2> val1 = { 1, 0 };
    EXPECT_EQ(coordinates, val1);

    EXPECT_TRUE(dims.incrementCoordinates(coordinates));
    std::array<int, 2> val2 = { 0, 1 };
    EXPECT_EQ(coordinates, val2);

    EXPECT_TRUE(dims.incrementCoordinates(coordinates));
    std::array<int, 2> val3 = { 1, 1 };
    EXPECT_EQ(coordinates, val3);

    EXPECT_TRUE(dims.incrementCoordinates(coordinates));
    std::array<int,2> val4 = { 0, 2 };
    EXPECT_EQ(coordinates, val4);

    EXPECT_TRUE(dims.incrementCoordinates(coordinates));
    std::array<int,2> val5 = { 1, 2 };
    EXPECT_EQ(coordinates, val5);

    EXPECT_FALSE(dims.incrementCoordinates(coordinates));
}

TEST(DimensionsTest, CreateCoordinates) {
    std::array<std::string,3> labels = { "X", "Y", "Z" };
    std::array<int,3> sizes = { 10, 20, 30 };
	std::array<int,3> increments = { 1, 1, 1 };
    Dimensions dims(labels, sizes, increments);

    std::list<std::pair<std::string, int>> coordList = { {"Y", 2}, {"Z", 3}, {"X", 1} };
    std::array<int,3> coordinates = dims.createCoordinates(coordList);

    std::array<int, 3> val = { 1, 2, 3 };
    EXPECT_EQ(coordinates, val);
}

TEST(DimensionsTest, CreateCoordinatesInvalidDimension) {
    std::array<std::string,3> labels = { "X", "Y", "Z" };
    std::array<int,3> sizes = { 10, 20, 30 };
	std::array<int,3> increments = { 1, 1, 1 };
    Dimensions dims(labels, sizes, increments);

    std::list<std::pair<std::string, int>> coordList = { {"X", 1}, {"Y", 2}, {"W", 3} };

    EXPECT_THROW(dims.createCoordinates(coordList), slideio::RuntimeError);
}

TEST(DimensionsTest, AreCoordsEqual) {
    slideio::Dimensions<3>::Coordinates coords1 = {1, 2, 3};
    slideio::Dimensions<3>::Coordinates coords2 = {1, 2, 3};
    slideio::Dimensions<3>::Coordinates coords3 = {3, 2, 1};

    EXPECT_TRUE(slideio::Dimensions<3>::areCoordsEqual(coords1, coords2));
    EXPECT_FALSE(slideio::Dimensions<3>::areCoordsEqual(coords1, coords3));
}

TEST(DimensionsTest, AreCoordsLess) {
    slideio::Dimensions<3>::Coordinates coords1 = {1, 2, 3};
    slideio::Dimensions<3>::Coordinates coords2 = {1, 2, 4};
    slideio::Dimensions<3>::Coordinates coords3 = {1, 3, 3};
    slideio::Dimensions<3>::Coordinates coords4 = { 1, 3, 3 };

    EXPECT_TRUE(slideio::Dimensions<3>::areCoordsLess(coords1, coords2));
    EXPECT_FALSE(slideio::Dimensions<3>::areCoordsLess(coords2, coords1));
    EXPECT_TRUE(slideio::Dimensions<3>::areCoordsLess(coords1, coords3));
    EXPECT_FALSE(slideio::Dimensions<3>::areCoordsLess(coords3, coords4));
}

TEST(DimensionsTest, AreCoordsGreater) {
    slideio::Dimensions<3>::Coordinates coords1 = {1, 2, 3};
    slideio::Dimensions<3>::Coordinates coords2 = {1, 2, 2};
    slideio::Dimensions<3>::Coordinates coords3 = {0, 2, 3};
	slideio::Dimensions<3>::Coordinates coords4 = { 0, 2, 3 };

    EXPECT_TRUE(slideio::Dimensions<3>::areCoordsGreater(coords1, coords2));
    EXPECT_FALSE(slideio::Dimensions<3>::areCoordsGreater(coords2, coords1));
    EXPECT_TRUE(slideio::Dimensions<3>::areCoordsGreater(coords1, coords3));
	EXPECT_FALSE(slideio::Dimensions<3>::areCoordsGreater(coords3, coords4));
}