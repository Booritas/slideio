// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/core/cvscene.hpp"
#include "slideio/base/exceptions.hpp"
#include "tests/testlib/testscene.hpp"
#include <memory>

using namespace slideio;

// Test fixture for channel attributes
class ChannelAttributesTest : public ::testing::Test {
protected:
    void SetUp() override {
        scene = std::make_shared<TestScene>();
        scene->setNumChannels(3);
    }

    std::shared_ptr<TestScene> scene;
};

TEST_F(ChannelAttributesTest, DefineChannelAttribute) {
    // Define a new attribute
    int attrIndex = scene->defineChannelAttribute("wavelength");
    EXPECT_EQ(attrIndex, 0);
    EXPECT_EQ(scene->getNumChannelAttributes(), 1);

    // Define another attribute
    int attrIndex2 = scene->defineChannelAttribute("exposure_time");
    EXPECT_EQ(attrIndex2, 1);
    EXPECT_EQ(scene->getNumChannelAttributes(), 2);

    // Defining the same attribute should return the same index
    int attrIndex3 = scene->defineChannelAttribute("wavelength");
    EXPECT_EQ(attrIndex3, 0);
    EXPECT_EQ(scene->getNumChannelAttributes(), 2);
}

TEST_F(ChannelAttributesTest, GetChannelAttributeIndex) {
    // Define attributes
    scene->defineChannelAttribute("wavelength");
    scene->defineChannelAttribute("exposure_time");
    scene->defineChannelAttribute("gain");

    // Get indices for defined attributes
    EXPECT_EQ(scene->getChannelAttributeIndex("wavelength"), 0);
    EXPECT_EQ(scene->getChannelAttributeIndex("exposure_time"), 1);
    EXPECT_EQ(scene->getChannelAttributeIndex("gain"), 2);

    // Get index for non-existent attribute
    EXPECT_EQ(scene->getChannelAttributeIndex("non_existent"), -1);
}

TEST_F(ChannelAttributesTest, SetAndGetChannelAttribute) {
    // Define attributes
    scene->defineChannelAttribute("wavelength");
    scene->defineChannelAttribute("exposure_time");

    // Set attributes for channel 0
    scene->setChannelAttribute(0, "wavelength", "488nm");
    scene->setChannelAttribute(0, "exposure_time", "100ms");

    // Get attributes for channel 0
    EXPECT_EQ(scene->getChannelAttribute(0, "wavelength"), "488nm");
    EXPECT_EQ(scene->getChannelAttribute(0, "exposure_time"), "100ms");

    // Set attributes for channel 1
    scene->setChannelAttribute(1, "wavelength", "561nm");
    scene->setChannelAttribute(1, "exposure_time", "150ms");

    // Verify channel 1 attributes
    EXPECT_EQ(scene->getChannelAttribute(1, "wavelength"), "561nm");
    EXPECT_EQ(scene->getChannelAttribute(1, "exposure_time"), "150ms");

    // Verify channel 0 attributes are unchanged
    EXPECT_EQ(scene->getChannelAttribute(0, "wavelength"), "488nm");
    EXPECT_EQ(scene->getChannelAttribute(0, "exposure_time"), "100ms");
}

TEST_F(ChannelAttributesTest, SetAttributeInvalidChannelIndex) {
    scene->defineChannelAttribute("wavelength");

    // Test invalid channel indices
    EXPECT_THROW(scene->setChannelAttribute(-1, "wavelength", "488nm"), slideio::RuntimeError);
    EXPECT_THROW(scene->setChannelAttribute(3, "wavelength", "488nm"), slideio::RuntimeError);
}

TEST_F(ChannelAttributesTest, GetAttributeInvalidChannelIndex) {
    scene->defineChannelAttribute("wavelength");
    scene->setChannelAttribute(0, "wavelength", "488nm");

    // Test invalid channel indices
    EXPECT_THROW(scene->getChannelAttribute(-1, "wavelength"), slideio::RuntimeError);
    EXPECT_THROW(scene->getChannelAttribute(3, "wavelength"), slideio::RuntimeError);
}

TEST_F(ChannelAttributesTest, GetAttributeNonExistent) {
    scene->defineChannelAttribute("wavelength");
    scene->setChannelAttribute(0, "wavelength", "488nm");

    // Test getting non-existent attribute
    EXPECT_THROW(scene->getChannelAttribute(0, "non_existent"), slideio::RuntimeError);
}

TEST_F(ChannelAttributesTest, GetChannelAttributes) {
    // Define and set multiple attributes
    scene->defineChannelAttribute("wavelength");
    scene->defineChannelAttribute("exposure_time");
    scene->defineChannelAttribute("gain");

    scene->setChannelAttribute(0, "wavelength", "488nm");
    scene->setChannelAttribute(0, "exposure_time", "100ms");
    scene->setChannelAttribute(0, "gain", "2.5");

    // Get all attributes for channel 0
    auto attributes = scene->getChannelAttributes(0);

    EXPECT_EQ(attributes.size(), 3);
    EXPECT_EQ(std::get<0>(attributes[0]), "wavelength");
    EXPECT_EQ(std::get<1>(attributes[0]), "488nm");
    EXPECT_EQ(std::get<0>(attributes[1]), "exposure_time");
    EXPECT_EQ(std::get<1>(attributes[1]), "100ms");
    EXPECT_EQ(std::get<0>(attributes[2]), "gain");
    EXPECT_EQ(std::get<1>(attributes[2]), "2.5");
}

TEST_F(ChannelAttributesTest, GetChannelAttributesInvalidIndex) {
    scene->defineChannelAttribute("wavelength");
    scene->setChannelAttribute(0, "wavelength", "488nm");

    // Test invalid channel indices
    EXPECT_THROW(scene->getChannelAttributes(-1), slideio::RuntimeError);
    EXPECT_THROW(scene->getChannelAttributes(3), slideio::RuntimeError);
}

TEST_F(ChannelAttributesTest, MultipleChannelsDifferentAttributes) {
    // Define attributes
    scene->defineChannelAttribute("wavelength");
    scene->defineChannelAttribute("exposure_time");
    scene->defineChannelAttribute("gain");

    // Set different attributes for different channels
    scene->setChannelAttribute(0, "wavelength", "488nm");
    scene->setChannelAttribute(0, "exposure_time", "100ms");
    scene->setChannelAttribute(0, "gain", "2.5");

    scene->setChannelAttribute(1, "wavelength", "561nm");
    scene->setChannelAttribute(1, "exposure_time", "150ms");
    scene->setChannelAttribute(1, "gain", "3.0");

    scene->setChannelAttribute(2, "wavelength", "640nm");
    scene->setChannelAttribute(2, "exposure_time", "200ms");
    scene->setChannelAttribute(2, "gain", "3.5");

    // Verify each channel has correct attributes
    EXPECT_EQ(scene->getChannelAttribute(0, "wavelength"), "488nm");
    EXPECT_EQ(scene->getChannelAttribute(0, "exposure_time"), "100ms");
    EXPECT_EQ(scene->getChannelAttribute(0, "gain"), "2.5");

    EXPECT_EQ(scene->getChannelAttribute(1, "wavelength"), "561nm");
    EXPECT_EQ(scene->getChannelAttribute(1, "exposure_time"), "150ms");
    EXPECT_EQ(scene->getChannelAttribute(1, "gain"), "3.0");

    EXPECT_EQ(scene->getChannelAttribute(2, "wavelength"), "640nm");
    EXPECT_EQ(scene->getChannelAttribute(2, "exposure_time"), "200ms");
    EXPECT_EQ(scene->getChannelAttribute(2, "gain"), "3.5");
}

TEST_F(ChannelAttributesTest, EmptyAttributeValues) {
    scene->defineChannelAttribute("wavelength");
    scene->defineChannelAttribute("comment");

    // Set empty values
    scene->setChannelAttribute(0, "wavelength", "");
    scene->setChannelAttribute(0, "comment", "");

    // Verify empty values are stored correctly
    EXPECT_EQ(scene->getChannelAttribute(0, "wavelength"), "");
    EXPECT_EQ(scene->getChannelAttribute(0, "comment"), "");
}

TEST_F(ChannelAttributesTest, OverwriteAttributeValue) {
    scene->defineChannelAttribute("wavelength");

    // Set initial value
    scene->setChannelAttribute(0, "wavelength", "488nm");
    EXPECT_EQ(scene->getChannelAttribute(0, "wavelength"), "488nm");

    // Overwrite value
    scene->setChannelAttribute(0, "wavelength", "561nm");
    EXPECT_EQ(scene->getChannelAttribute(0, "wavelength"), "561nm");
}

TEST_F(ChannelAttributesTest, NumChannelAttributes) {
    EXPECT_EQ(scene->getNumChannelAttributes(), 0);

    scene->defineChannelAttribute("wavelength");
    EXPECT_EQ(scene->getNumChannelAttributes(), 1);

    scene->defineChannelAttribute("exposure_time");
    EXPECT_EQ(scene->getNumChannelAttributes(), 2);

    scene->defineChannelAttribute("gain");
    EXPECT_EQ(scene->getNumChannelAttributes(), 3);

    // Re-defining should not increase count
    scene->defineChannelAttribute("wavelength");
    EXPECT_EQ(scene->getNumChannelAttributes(), 3);
}

