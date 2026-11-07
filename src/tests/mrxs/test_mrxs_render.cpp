// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// Decoding and rendering (design sections 6 and 7) through the CVScene read entry points,
// on the committed fixtures: probe pixels, background, the level/scaled read identity,
// band independence, channel selection, and payloads that must fail when read.
#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include "slideio/drivers/mrxs/mrxsimagedriver.hpp"
#include "slideio/drivers/mrxs/mrxsdataset.hpp"
#include "slideio/drivers/mrxs/mrxsrenderer.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/core/exceptions.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace slideio::mrxs;

namespace
{
    const fs::path dataDir() { return fs::path(MRXS_TEST_DATA_DIR); }

    json load(const fs::path& path)
    {
        std::ifstream in(path);
        return json::parse(in);
    }

    std::vector<std::string> validFixtures()
    {
        const json manifest = load(dataDir() / "manifest.json");
        std::vector<std::string> names;
        for (const auto& name : manifest["fixtures"]) {
            if (!load(dataDir() / (name.get<std::string>() + ".expected.json")).contains("error")) {
                names.push_back(name.get<std::string>());
            }
        }
        return names;
    }

    std::string entry(const std::string& name) { return (dataDir() / (name + ".mrxs")).u8string(); }

    std::shared_ptr<slideio::CVScene> sceneOf(const std::string& name)
    {
        slideio::MRXSImageDriver driver;
        return driver.openFile(entry(name))->getScene(0);
    }

    cv::Mat readLevel(slideio::CVScene& scene, int level, const cv::Rect& rect, const cv::Size& size,
                      const std::vector<int>& channels = {})
    {
        cv::Mat out;
        scene.readResampledLevelBlockChannels(level, rect, size, channels, out);
        return out;
    }

    bool identical(const cv::Mat& a, const cv::Mat& b)
    {
        return a.size() == b.size() && a.type() == b.type() && cv::norm(a, b, cv::NORM_INF) == 0;
    }

    template <typename F>
    std::string errorOf(F f)
    {
        try {
            f();
        }
        catch (const slideio::RuntimeError& e) {
            return e.what();
        }
        return "";
    }
}

TEST(MRXSRender, everyProbePixelHasItsColour)
{
    int probes = 0;
    for (const std::string& name : validFixtures()) {
        SCOPED_TRACE(name);
        const json expected = load(dataDir() / (name + ".expected.json"));
        const int tolerance = expected["pixel_tolerance"].get<int>();
        auto scene = sceneOf(name);
        for (size_t l = 0; l < expected["levels"].size(); ++l) {
            const json& level = expected["levels"][l];
            const int w = level["width"].get<int>(), h = level["height"].get<int>();
            const cv::Mat image = readLevel(*scene, static_cast<int>(l), cv::Rect(0, 0, w, h), cv::Size(w, h));
            ASSERT_EQ(CV_8UC3, image.type());
            for (const char* kind : {"probes", "holes"}) {
                for (const json& p : level[kind]) {
                    const cv::Vec3b got = image.at<cv::Vec3b>(p["y"].get<int>(), p["x"].get<int>());
                    for (int c = 0; c < 3; ++c) {
                        EXPECT_LE(std::abs(int(got[c]) - p["rgb"][c].get<int>()), tolerance)
                            << kind << " level " << l << " (" << p["x"] << "," << p["y"] << ") channel " << c;
                    }
                    ++probes;
                }
            }
        }
    }
    EXPECT_GE(probes, 100);
}

TEST(MRXSRender, pixelsOutsideTheCanvasTakeTheBackground)
{
    for (const auto& [name, rgb] : {std::make_pair(std::string("rawmap-d2"), cv::Vec3b(240, 240, 240)),
                                    std::make_pair(std::string("nomap-d1"), cv::Vec3b(0x99, 0x66, 0x33))}) {
        SCOPED_TRACE(name);
        auto scene = sceneOf(name);
        const cv::Rect canvas = scene->getRect();
        // A block hanging 4 pixels past the right and bottom edges.
        const cv::Rect rect(canvas.width - 6, canvas.height - 6, 10, 10);
        const cv::Mat image = readLevel(*scene, 0, rect, rect.size());
        for (int y = 0; y < 10; ++y) {
            for (int x = 0; x < 10; ++x) {
                if (x >= 6 || y >= 6) {
                    EXPECT_EQ(rgb, image.at<cv::Vec3b>(y, x)) << "(" << x << "," << y << ")";
                }
            }
        }
    }
}

TEST(MRXSRender, theBackgroundIsTheSlidesFillColour)
{
    // getChannelBackground carries IMAGE_FILL_COLOR_BGR: low byte red.
    auto scene = sceneOf("nomap-d1");
    EXPECT_EQ(0x99, scene->getChannelBackground(0));
    EXPECT_EQ(0x66, scene->getChannelBackground(1));
    EXPECT_EQ(0x33, scene->getChannelBackground(2));
}

TEST(MRXSRender, aLevelReadEqualsTheEquivalentScaledReadBitForBit)
{
    int compared = 0;
    for (const std::string name : {"rawmap-d2", "saved-c0-4", "nomap-overlap-d2", "zlibmap-d2-jpeg", "nomap-d1"}) {
        SCOPED_TRACE(name);
        auto scene = sceneOf(name);
        for (int level = 0; level < scene->getNumZoomLevels(); ++level) {
            const slideio::LevelInfo* info = scene->getZoomLevelInfo(level);
            const int d = static_cast<int>(std::lround(1.0 / info->getScale()));
            const int w = info->getSize().width, h = info->getSize().height;
            for (const cv::Rect& levelRect : {cv::Rect(0, 0, w, h), cv::Rect(1, 2, std::max(1, w / 2), std::max(1, h / 2)),
                                              cv::Rect(w / 3, h / 4, std::max(1, w / 3), std::max(1, h / 2))}) {
                for (const double shrink : {1.0, 0.5}) {
                    const cv::Size size(std::max(1, int(levelRect.width * shrink)), std::max(1, int(levelRect.height * shrink)));
                    if (shrink != 1.0 && level + 1 < scene->getNumZoomLevels()) {
                        continue;   // a smaller output would select a coarser level, not this one
                    }
                    const cv::Rect sceneRect(levelRect.x * d, levelRect.y * d, levelRect.width * d, levelRect.height * d);
                    cv::Mat scaled;
                    scene->readResampledBlockChannels(sceneRect, size, {}, scaled);
                    EXPECT_TRUE(identical(readLevel(*scene, level, levelRect, size), scaled))
                        << "level " << level << " rect " << levelRect << " size " << size;
                    ++compared;
                }
            }
        }
    }
    EXPECT_GE(compared, 30);
}

TEST(MRXSRender, aZoomWithinOnePercentOfALevelReadsThatLevel)
{
    // rawmap-d2 level 1 has downsample 2. A request at zoom 0.504 is 0.8% off it and must
    // read level 1, exactly as SVS and PKE do through Tools::findZoomLevel.
    auto scene = sceneOf("rawmap-d2");
    const cv::Rect sceneRect(0, 0, 250, 250);
    const cv::Size size(126, 126);
    cv::Mat scaled;
    scene->readResampledBlockChannels(sceneRect, size, {}, scaled);
    EXPECT_TRUE(identical(readLevel(*scene, 1, cv::Rect(0, 0, 125, 125), size), scaled));
}

TEST(MRXSRender, theBandSizeDoesNotChangeAnyPixel)
{
    for (const std::string name : {"rawmap-d2", "nomap-overlap-d2", "saved-c0-4"}) {
        SCOPED_TRACE(name);
        const auto dataset = MRXSDataset::open(entry(name));
        const Renderer renderer(*dataset);
        for (int level = 0; level < static_cast<int>(dataset->geometry.levels.size()); ++level) {
            const LevelGeometry& lg = dataset->geometry.levels[level];
            const cv::Rect rect(0, 0, static_cast<int>(lg.width), static_cast<int>(lg.height));
            cv::Mat whole, banded;
            renderer.render(level, rect, whole);
            renderer.render(level, rect, banded, /*bandPixels*/ 37);
            EXPECT_TRUE(identical(whole, banded)) << "level " << level;
        }
    }
}

TEST(MRXSRender, channelsComeInTheCallersOrder)
{
    auto scene = sceneOf("rawmap-d2");
    const cv::Rect rect(5, 5, 30, 20);
    const cv::Mat rgb = readLevel(*scene, 0, rect, rect.size());
    const cv::Mat picked = readLevel(*scene, 0, rect, rect.size(), {2, 0});
    ASSERT_EQ(CV_8UC2, picked.type());
    std::vector<cv::Mat> planes, pickedPlanes;
    cv::split(rgb, planes);
    cv::split(picked, pickedPlanes);
    EXPECT_TRUE(identical(planes[2], pickedPlanes[0]));
    EXPECT_TRUE(identical(planes[0], pickedPlanes[1]));
}

TEST(MRXSRender, theSameSlideRendersTheSameWhateverItsPageLayout)
{
    // pages-out-of-order is nomap-d1 with its index pages chained out of file order.
    auto a = sceneOf("nomap-d1");
    auto b = sceneOf("pages-out-of-order");
    const cv::Rect rect = a->getRect();
    EXPECT_TRUE(identical(readLevel(*a, 0, rect, rect.size()), readLevel(*b, 0, rect, rect.size())));
}

TEST(MRXSRender, badPayloadsFailWhenRead)
{
    for (const std::string name : {"codec-mismatch", "bmp-not-24bit", "wrong-image-size"}) {
        SCOPED_TRACE(name);
        auto scene = sceneOf(name);
        const std::string message = errorOf([&] {
            readLevel(*scene, 0, scene->getRect(), scene->getRect().size());
        });
        EXPECT_EQ(0u, message.find("MRXS corrupt:")) << message;
    }
}

TEST(MRXSRender, theDecoderEnforcesTheBmp24Profile)
{
    // An 8-bit palette BMP declared as BMP24 must not decode quietly.
    const auto dataset = MRXSDataset::open(entry("bmp-not-24bit"));
    const ImageRecord* bad = nullptr;
    for (const ImageRecord& r : dataset->structure.levels[0]) {
        if (r.x == 1 && r.y == 0) bad = &r;
    }
    ASSERT_NE(nullptr, bad);
    const std::vector<uint8_t> bytes = readPayload(dataset->structure, bad->payload);
    const std::string message = errorOf([&] { decodeImage(bytes, ImageFormat::Bmp24, 13, 7, "test payload"); });
    EXPECT_EQ(0u, message.find("MRXS corrupt:")) << message;
}
