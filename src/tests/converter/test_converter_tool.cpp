// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// The slideio-converter tool's open options (-o key=value): parsing, and that they
// reach the slide the tool opens.
#include <gtest/gtest.h>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <tuple>
#include <vector>
#include "tools/converter/sceneconverter.hpp"
#include "slideio/slideio/slideio.hpp"
#include "slideio/core/rect.hpp"
#include "slideio/core/range.hpp"

namespace
{
    std::string errorOf(const std::function<void()>& call)
    {
        try {
            call();
        }
        catch (const std::exception& e) {
            return e.what();
        }
        return "";
    }

    std::tuple<int, int, int, int> convertedRect(const std::map<std::string, std::string>& options,
                                                 const std::string& name)
    {
        const std::string input = (std::filesystem::path(MRXS_FIXTURE_DIR) / "offset-content.mrxs").u8string();
        const std::string output = (std::filesystem::temp_directory_path() / name).u8string();
        convertFile(input, output, 0, 5.0, 256, -1, "AUTO", "SVS", "Jpeg", 95,
                    slideio::Rect(), slideio::Range(), slideio::Range(), slideio::Range(),
                    true, false, true, 10, 1, 1, options);
        const auto rect = slideio::openSlide(output, "SVS")->getScene(0)->getRect();
        std::filesystem::remove(output);
        return rect;
    }
}

TEST(ConverterTool, parsesKeyValueOptions)
{
    EXPECT_TRUE(parseOpenOptions({}).empty());
    const std::map<std::string, std::string> expected{{"mrxs.crop_to_content", "false"}, {"a.b", "x=y"}};
    EXPECT_EQ(expected, parseOpenOptions({"mrxs.crop_to_content=false", "a.b=x=y"}));
}

TEST(ConverterTool, rejectsAnOptionWithoutAnEqualsSignOrKey)
{
    for (const std::string bad : {"mrxs.crop_to_content", "=false"}) {
        const std::string message = errorOf([&] { parseOpenOptions({bad}); });
        EXPECT_NE(std::string::npos, message.find("key=value")) << bad << ": " << message;
        EXPECT_NE(std::string::npos, message.find(bad)) << bad << ": " << message;
    }
}

TEST(ConverterTool, rejectsAKeyGivenTwice)
{
    const std::string message = errorOf([] {
        parseOpenOptions({"mrxs.crop_to_content=false", "mrxs.crop_to_content=true"});
    });
    EXPECT_NE(std::string::npos, message.find("given twice")) << message;
}

TEST(ConverterTool, theOptionsReachTheSlideTheToolOpens)
{
    // offset-content: a 72 x 54 canvas whose scanned area is 45 x 45.
    EXPECT_EQ(std::make_tuple(0, 0, 45, 45), convertedRect({}, "slideio_tool_cropped.svs"));
    EXPECT_EQ(std::make_tuple(0, 0, 72, 54),
              convertedRect({{"mrxs.crop_to_content", "false"}}, "slideio_tool_whole.svs"));
}
