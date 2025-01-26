// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <opencv2/imgproc.hpp>

#include "slideio/core/tools/tools.hpp"
#include "tests/testlib/testtools.hpp"
#include "tests/testlib/testtools.hpp"
#include <filesystem>

#include "slideio/base/exceptions.hpp"
#include "slideio/core/tools/tempfile.hpp"


TEST(Tools, matchPattern) {
#if defined(WIN32)    
    EXPECT_TRUE(slideio::Tools::matchPattern("c:\\abs.ad.czi","*.czi"));
    EXPECT_TRUE(slideio::Tools::matchPattern("c:\\abs.ad.czi","*.tiff;*.czi"));
    EXPECT_FALSE(slideio::Tools::matchPattern("c:\\abs.ad.czi","*.tiff;*.aczi"));
    EXPECT_TRUE(slideio::Tools::matchPattern("c:\\abs.ad.OME.TIFF","*.atiff;*.aczi;*.ome.tiff"));
#else
    EXPECT_TRUE(slideio::Tools::matchPattern("ad.czi","*.czi"));
    EXPECT_TRUE(slideio::Tools::matchPattern("/root/abs.ad.czi","*.czi"));
    EXPECT_TRUE(slideio::Tools::matchPattern("/root/abs.ad.czi","*.tiff;*.czi"));
    EXPECT_FALSE(slideio::Tools::matchPattern("/root/abs.ad.czi","*.tiff;*.aczi"));
    EXPECT_TRUE(slideio::Tools::matchPattern("/root/abs.ad.ome.tiff","*.atiff;*.aczi;*.ome.tiff"));
#endif
}
TEST(Tools, findZoomLevel)
{
    struct ZoomLevel
    {
        double zoom;
        double getZoom() const { return zoom; }
    };
    std::vector<ZoomLevel> levels;
    levels.resize(10);
    double zoom = 1.;
    for (auto& level : levels)
    {
        level.zoom = zoom;
        zoom /= 2.;
    }
    double lastZoom = levels.back().zoom;
    auto zoomFunct = [&levels](int level)
    {
        return levels[level].zoom;
    };
    const int numLevels = static_cast<int>(levels.size());
    EXPECT_EQ(slideio::Tools::findZoomLevel(2., numLevels, zoomFunct), 0);
    EXPECT_EQ(slideio::Tools::findZoomLevel(lastZoom, numLevels, zoomFunct), 9);
    EXPECT_EQ(slideio::Tools::findZoomLevel(lastZoom * 2, numLevels, zoomFunct), 8);
    EXPECT_EQ(slideio::Tools::findZoomLevel(lastZoom / 2, numLevels, zoomFunct), 9);
    EXPECT_EQ(slideio::Tools::findZoomLevel(0.5, numLevels, zoomFunct), 1);
    EXPECT_EQ(slideio::Tools::findZoomLevel(0.501, numLevels, zoomFunct), 1);
    EXPECT_EQ(slideio::Tools::findZoomLevel(0.499, numLevels, zoomFunct), 1);
    EXPECT_EQ(slideio::Tools::findZoomLevel(0.25, numLevels, zoomFunct), 2);
    EXPECT_EQ(slideio::Tools::findZoomLevel(0.125, numLevels, zoomFunct), 3);
    EXPECT_EQ(slideio::Tools::findZoomLevel(0.55, numLevels, zoomFunct), 0);
    EXPECT_EQ(slideio::Tools::findZoomLevel(0.45, numLevels, zoomFunct), 1);
    EXPECT_EQ(slideio::Tools::findZoomLevel(0.1, numLevels, zoomFunct), 3);
}

TEST(Tools, convert12BitsTo16Bits2vals)
{
    int16_t src1 = 0xE0F;
    int16_t src2 = 0xF0E;
    int32_t srcB = src1 | (src2 << 12);
    uint16_t trg[2] = { 0 };
    slideio::Tools::convert12BitsTo16Bits((uint8_t*)&srcB, trg, 2);
    EXPECT_EQ(src1, trg[0]);
    EXPECT_EQ(src2, trg[1]);
}

TEST(Tools, convert12BitsTo16Bits3vals)
{
    int16_t src1 = 0xE0F;
    int16_t src2 = 0xF0E;
    int16_t src3 = 0xA0B;
    uint8_t src[8];
    int32_t src32= src1 | (src2 << 12);
    memcpy(src, &src32, sizeof(src32));
    src32 = src3;
    memcpy(src+3, &src32, sizeof(src32));
    uint16_t trg[3] = { 0 };
    slideio::Tools::convert12BitsTo16Bits(src, trg, 3);
    EXPECT_EQ(src1, trg[0]);
    EXPECT_EQ(src2, trg[1]);
    EXPECT_EQ(src3, trg[2]);
}

TEST(Tools, extractChannels11)
{
    // Create a sample input image
    cv::Mat sourceRaster = cv::Mat::zeros(100, 100, CV_8UC3);
    cv::circle(sourceRaster, cv::Point(50, 50), 30, cv::Scalar(255, 255, 255), -1);

    // Test case 1: Extract all channels
    cv::Mat output1;
    slideio::Tools::extractChannels(sourceRaster, {}, output1);
    EXPECT_EQ(sourceRaster.size(), output1.size());
    EXPECT_EQ(sourceRaster.type(), output1.type());
    EXPECT_EQ(TestTools::countNonZero(output1 == sourceRaster), sourceRaster.total()*sourceRaster.channels());

    // Test case 2: Extract specific channels
    std::vector<int> channels = {0, 2};
    cv::Mat output2;
    slideio::Tools::extractChannels(sourceRaster, channels, output2);
    EXPECT_EQ(sourceRaster.size(), output2.size());
    EXPECT_EQ(CV_8UC2, output2.type());
    cv::Mat originalChannnel0;
    cv::extractChannel(sourceRaster, originalChannnel0, 0);
    cv::Mat originalChannel2;
    cv::extractChannel(sourceRaster, originalChannel2, 2);
    cv::Mat originalChannels[] = {originalChannnel0, originalChannel2};
    cv::Mat expected;
    cv::merge(originalChannels, 2, expected);
    EXPECT_EQ(TestTools::countNonZero(output2 == expected), expected.total()*expected.channels());
}

TEST(Tools, replaceAll)
{
    // single character replacement
    std::string str = "This is a test string";
    slideio::Tools::replaceAll(str, " ", "_");
    EXPECT_EQ(str, "This_is_a_test_string");
    // empty string replacement
    str = "";
    slideio::Tools::replaceAll(str, " ", "_");
    EXPECT_EQ(str, "");
    // multiple character replacement
    str = "This is a test string";
    slideio::Tools::replaceAll(str, " is", "_was");
    EXPECT_EQ(str, "This_was a test string");
    // quoted string replacement
    str = "\"This is a test string\"";
    slideio::Tools::replaceAll(str, "\"", "'");
    EXPECT_EQ(str, "'This is a test string'");
    // not found replacement
    str = "This is a test string";
    slideio::Tools::replaceAll(str, "notfound", "_");
    EXPECT_EQ(str, "This is a test string");
    // backslash replacement
    str = "This\\is\\a\\test\\string";
    slideio::Tools::replaceAll(str, "\\", "/");
    EXPECT_EQ(str, "This/is/a/test/string");   
}

TEST(Tools, split)
{
    // Empty string
    std::vector<std::string> tokens = slideio::Tools::split("", ',');
    EXPECT_TRUE(tokens.empty());

    // Single token
    tokens = slideio::Tools::split("token", ',');
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0], "token");

    // Multiple tokens
    tokens = slideio::Tools::split("token1,token2,token3", ',');
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0], "token1");
    EXPECT_EQ(tokens[1], "token2");
    EXPECT_EQ(tokens[2], "token3");

    // Different delimiter
    tokens = slideio::Tools::split("token1|token2|token3", '|');
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0], "token1");
    EXPECT_EQ(tokens[1], "token2");
    EXPECT_EQ(tokens[2], "token3");

    // trailing delimiters
    tokens = slideio::Tools::split("token1,token2,", ',');
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0], "token1");
    EXPECT_EQ(tokens[1], "token2");
    EXPECT_EQ(tokens[2], "");    

    // Leading delimiters
    tokens = slideio::Tools::split(",token1,token2", ',');
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0], "");
    EXPECT_EQ(tokens[1], "token1");
    EXPECT_EQ(tokens[2], "token2");
}

TEST(Tools, unique_path) {
    // No Placeholders
    {
        std::string model = "testfile.txt";
        std::filesystem::path path = slideio::TempFile::unique_path(model);
        EXPECT_EQ(path.string(), model);
    }

    // With Placeholders
    {
        std::string model = "testfile_%%%%.txt";
        std::filesystem::path path = slideio::TempFile::unique_path(model);
        std::string pathStr = path.string();

        // Check that the length is correct
        EXPECT_EQ(pathStr.size(), model.size());

        // Check that the non-placeholder parts are correct
        EXPECT_EQ(pathStr.substr(0, 9), "testfile_");
        EXPECT_EQ(pathStr.substr(13), ".txt");

        // Check that the placeholders are replaced with alphanumeric characters
        for (size_t i = 9; i < 13; ++i) {
            EXPECT_TRUE(isalnum(pathStr[i]));
        }
    }

    // Empty Model
    {
        std::string model = "";
        std::filesystem::path path = slideio::TempFile::unique_path(model);
        EXPECT_EQ(path.string(), model);
    }

    // All Placeholders
    {
        std::string model = "%%%%";
        std::filesystem::path path = slideio::TempFile::unique_path(model);
        std::string pathStr = path.string();

        // Check that the length is correct
        EXPECT_EQ(pathStr.size(), model.size());

        // Check that the placeholders are replaced with alphanumeric characters
        for (char c : pathStr) {
            EXPECT_TRUE(isalnum(c));
        }
    }
}

#if defined(WIN32)
  TEST(Tools, toWstring) {
    {
        std::string utf8Str = "";
        std::wstring expected = L"";
        std::wstring result = slideio::Tools::toWstring(utf8Str);
        EXPECT_EQ(result, expected);
    }
    {
        std::string utf8Str = "Hello, World!";
        std::wstring expected = L"Hello, World!";
        std::wstring result = slideio::Tools::toWstring(utf8Str);
        EXPECT_EQ(result, expected);
    }
    {
        std::string utf8Str = "\xE3\x81\x93\xE3\x82\x93\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\xAF\xE4\xB8\x96\xE7\x95\x8C";
        std::wstring expected = L"\u3053\u3093\u306B\u3061\u306F\u4E16\u754C";
        std::wstring result = slideio::Tools::toWstring(utf8Str);
        EXPECT_EQ(result, expected);
    }
    {
        std::string utf8Str = "\xC3\x28"; // Invalid UTF-8 sequence
        EXPECT_THROW(slideio::Tools::toWstring(utf8Str), slideio::RuntimeError);
    }
}

#endif
