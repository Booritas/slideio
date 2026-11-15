// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>

#include "slideio/core/openoptions.hpp"
#include "slideio/core/imagedriver.hpp"
#include "slideio/core/exceptions.hpp"

using namespace slideio;

namespace
{
    // Takes no options of its own: what every driver but MRXS is.
    class TestDriver : public ImageDriver
    {
    public:
        using ImageDriver::openFile;
        std::string getID() const override { return "TESTDRV"; }
        std::shared_ptr<CVSlide> openFile(const std::string&) override {
            ++calls;
            return nullptr;
        }
        std::string getFileSpecs() const override { return "*.test"; }
        int calls = 0;
    };
}

TEST(OpenOptions, splitsAtTheFirstDot)
{
    EXPECT_EQ("mrxs", splitOptionKey("mrxs.crop_to_content").driver);
    EXPECT_EQ("crop_to_content", splitOptionKey("mrxs.crop_to_content").name);
    EXPECT_EQ("a", splitOptionKey("a.b.c").driver);
    EXPECT_EQ("b.c", splitOptionKey("a.b.c").name);
    EXPECT_EQ("", splitOptionKey("nodot").driver);
    EXPECT_EQ("nodot", splitOptionKey("nodot").name);
    EXPECT_EQ("", splitOptionKey(".x").driver);
}

TEST(OpenOptions, keyOfLowercasesTheDriverId)
{
    EXPECT_EQ("ometiff.x", optionKeyOf("OMETIFF", "x"));
}

TEST(OpenOptions, booleansAcceptFourSpellingsInAnyCase)
{
    for (auto t : {"true", "TRUE", "True", "1"}) {
        EXPECT_TRUE(parseBoolOption("k.b", t)) << t;
    }
    for (auto f : {"false", "False", "0"}) {
        EXPECT_FALSE(parseBoolOption("k.b", f)) << f;
    }
    for (auto bad : {"yes", "", "2", " true"}) {
        try {
            parseBoolOption("k.b", bad);
            FAIL() << "'" << bad << "' should have been rejected";
        }
        catch (const RuntimeError& e) {
            const std::string message(e.what());
            EXPECT_NE(std::string::npos, message.find("is not a boolean")) << message;
            EXPECT_NE(std::string::npos, message.find("k.b")) << message;
        }
    }
}

TEST(OpenOptions, aDriverWithoutOptionsForwardsAnEmptyMap)
{
    TestDriver driver;
    driver.openFile("p", DriverOptions{});
    EXPECT_EQ(1, driver.calls);
}

TEST(OpenOptions, aDriverWithoutOptionsRejectsAnyKeyBeforeOpening)
{
    TestDriver driver;
    try {
        driver.openFile("p", DriverOptions{{"x", "1"}});
        FAIL() << "the key should have been rejected";
    }
    catch (const RuntimeError& e) {
        const std::string message(e.what());
        EXPECT_NE(std::string::npos, message.find("has no open option 'testdrv.x'")) << message;
    }
    EXPECT_EQ(0, driver.calls);
}
