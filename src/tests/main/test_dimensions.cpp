// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/dimensions.hpp"
#include "slideio/base/exceptions.hpp"
#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <list>


using namespace slideio;

TEST(DimensionsTest, Constructor) {
    std::vector<std::string> labels = { "X", "Y", "Z" };
    std::vector<int> sizes = { 10, 20, 30 };
	std::vector<int> increments = { 1, 1, 1 };
    Dimensions dims(labels, sizes, increments);

    EXPECT_EQ(dims.getOrder(), labels);
    EXPECT_EQ(dims.getSizes(), sizes);
	for (int index = 0; index < labels.size(); ++index) {
		EXPECT_EQ(dims.getDimensionSize(labels[index]), sizes[index]);
		EXPECT_EQ(dims.getDimensionIndex(labels[index]), index);
	}
}

TEST(DimensionsTest, ConstructorMismatch) {
    std::vector<std::string> labels = { "X", "Y" };
    std::vector<int> sizes = { 10, 20, 30 };
	std::vector<int> increments = { 1, 1, 1 };

    EXPECT_THROW(Dimensions dims(labels, sizes, increments), slideio::RuntimeError);
}

TEST(DimensionsTest, IncrementCoordinates) {
    std::vector<std::string> labels = { "X", "Y" };
    std::vector<int> sizes = { 2, 3 };
	std::vector<int> increments = { 1, 1 };
    Dimensions dims(labels, sizes, increments);

    std::vector<int> coordinates = { 0, 0 };
    EXPECT_TRUE(dims.incrementCoordinates(coordinates));
    EXPECT_EQ(coordinates, std::vector<int>({ 1, 0 }));

    EXPECT_TRUE(dims.incrementCoordinates(coordinates));
    EXPECT_EQ(coordinates, std::vector<int>({ 0, 1 }));

    EXPECT_TRUE(dims.incrementCoordinates(coordinates));
    EXPECT_EQ(coordinates, std::vector<int>({ 1, 1 }));

    EXPECT_TRUE(dims.incrementCoordinates(coordinates));
    EXPECT_EQ(coordinates, std::vector<int>({ 0, 2 }));

    EXPECT_TRUE(dims.incrementCoordinates(coordinates));
    EXPECT_EQ(coordinates, std::vector<int>({ 1, 2 }));

    EXPECT_FALSE(dims.incrementCoordinates(coordinates));
}

TEST(DimensionsTest, CreateCoordinates) {
    std::vector<std::string> labels = { "X", "Y", "Z" };
    std::vector<int> sizes = { 10, 20, 30 };
	std::vector<int> increments = { 1, 1, 1 };
    Dimensions dims(labels, sizes, increments);

    std::list<std::pair<std::string, int>> coordList = { {"Y", 2}, {"Z", 3}, {"X", 1} };
    std::vector<int> coordinates = dims.createCoordinates(coordList);
    EXPECT_EQ(coordinates, std::vector<int>({ 1, 2, 3 }));
}

TEST(DimensionsTest, CreateCoordinatesMismatch) {
    std::vector<std::string> labels = { "X", "Y", "Z" };
    std::vector<int> sizes = { 10, 20, 30 };
	std::vector<int> increments = { 1, 1, 1 };
    Dimensions dims(labels, sizes, increments);

    std::list<std::pair<std::string, int>> coordList = { {"X", 1}, {"Y", 2} };

    EXPECT_THROW(dims.createCoordinates(coordList), slideio::RuntimeError);
}

TEST(DimensionsTest, CreateCoordinatesInvalidDimension) {
    std::vector<std::string> labels = { "X", "Y", "Z" };
    std::vector<int> sizes = { 10, 20, 30 };
	std::vector<int> increments = { 1, 1, 1 };
    Dimensions dims(labels, sizes, increments);

    std::list<std::pair<std::string, int>> coordList = { {"X", 1}, {"Y", 2}, {"W", 3} };

    EXPECT_THROW(dims.createCoordinates(coordList), slideio::RuntimeError);
}