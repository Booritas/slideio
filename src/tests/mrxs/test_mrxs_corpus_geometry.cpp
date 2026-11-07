// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// Geometry of the real integration bundles against OpenSlide 4.0.1: level sizes and
// populated bounds from the reference manifest written by
// auxfiles/mrxs-openslide-reference.py into SLIDEIO_IMAGES_PATH/mirax/reference.
#include <gtest/gtest.h>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include "slideio/drivers/mrxs/mrxsgeometry.hpp"
#include "tests/testlib/testtools.hpp"

using json = nlohmann::json;
using namespace slideio::mrxs;

TEST(MRXSCorpusGeometry, levelSizesAndBoundsMatchOpenSlide)
{
    const std::string manifestPath = TestTools::getTestImagePath("mirax/reference", "manifest.json");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(manifestPath);
    std::ifstream in(manifestPath);
    const json manifest = json::parse(in);
    int checked = 0;
    for (const auto& item : manifest["bundles"].items()) {
        const std::string& rel = item.key();
        if (rel.find("2401582-38") != std::string::npos) {
            continue;   // five-channel: rejected as unsupported
        }
        SCOPED_TRACE(rel);
        const std::string path = TestTools::getTestImagePath("mirax", rel);
        SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
        const SlideStructure slide = readSlideStructure(path);
        const Geometry geometry = Geometry::build(slide);
        const json& expected = item.value();
        ASSERT_EQ(expected["levels"].size(), geometry.levels.size());
        for (size_t l = 0; l < geometry.levels.size(); ++l) {
            EXPECT_EQ(expected["levels"][l][0].get<int64_t>(), geometry.levels[l].width) << "level " << l;
            EXPECT_EQ(expected["levels"][l][1].get<int64_t>(), geometry.levels[l].height) << "level " << l;
            EXPECT_DOUBLE_EQ(expected["downsamples"][l].get<double>(), double(geometry.levels[l].downsample));
        }
        // OpenSlide's bounds: the level-0 drawables' union rounded outward, unclipped.
        ASSERT_TRUE(geometry.bounds0.has_value());
        const cv::Rect2d& b = *geometry.bounds0;
        const int64_t x0 = int64_t(std::floor(b.x)), y0 = int64_t(std::floor(b.y));
        const int64_t x1 = int64_t(std::ceil(b.x + b.width)), y1 = int64_t(std::ceil(b.y + b.height));
        EXPECT_EQ(expected["bounds"][0].get<int64_t>(), x0);
        EXPECT_EQ(expected["bounds"][1].get<int64_t>(), y0);
        EXPECT_EQ(expected["bounds"][2].get<int64_t>(), x1 - x0);
        EXPECT_EQ(expected["bounds"][3].get<int64_t>(), y1 - y0);
        ++checked;
    }
    EXPECT_EQ(8, checked);
}

// The measurement design section 3 asks for at the geometry milestone: index memory and
// open time on the largest (million-descriptor) bundle. Not a regression test -- run it by
// hand with MRXS_BENCHMARK_BUNDLE set to the bundle's Slidedat.ini and
// --gtest_also_run_disabled_tests.
TEST(MRXSCorpusGeometry, DISABLED_measureTheLargestBundle)
{
    const char* path = std::getenv("MRXS_BENCHMARK_BUNDLE");
    if (!path) {
        GTEST_SKIP() << "MRXS_BENCHMARK_BUNDLE is not set";
    }
    using clock = std::chrono::steady_clock;
    const auto t0 = clock::now();
    const SlideStructure slide = readSlideStructure(path);
    const auto t1 = clock::now();
    const Geometry geometry = Geometry::build(slide);
    const auto t2 = clock::now();
    size_t records = 0, drawables = 0, geometryBytes = 0;
    for (size_t l = 0; l < geometry.levels.size(); ++l) {
        records += slide.levels[l].size();
        drawables += geometry.levels[l].drawables().size();
        geometryBytes += geometry.levels[l].memoryBytes();
    }
    const auto ms = [](auto a, auto b) { return std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count(); };
    std::cout << "records " << records << " (" << records * sizeof(ImageRecord) / (1 << 20) << " MiB), drawables "
              << drawables << ", geometry " << geometryBytes / (1 << 20) << " MiB; index read " << ms(t0, t1)
              << " ms, geometry " << ms(t1, t2) << " ms" << std::endl;
}
