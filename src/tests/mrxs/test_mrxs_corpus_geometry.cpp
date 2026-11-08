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
#include <map>
#include <algorithm>
#include <vector>
#include <nlohmann/json.hpp>
#include <string>
#include "slideio/drivers/mrxs/mrxsgeometry.hpp"
#include "slideio/drivers/mrxs/mrxsimagedriver.hpp"
#include "slideio/drivers/mrxs/mrxsdataset.hpp"
#include "slideio/drivers/mrxs/mrxsscene.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/cvslide.hpp"
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

TEST(MRXSCorpusGeometry, associatedImagesMatchOpenSlide)
{
    // Names and sizes only: no label pixels are decoded or compared.
    const std::string manifestPath = TestTools::getTestImagePath("mirax/reference", "manifest.json");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(manifestPath);
    std::ifstream in(manifestPath);
    const json manifest = json::parse(in);
    const std::map<std::string, std::string> openslideName = {{"Macro", "macro"}, {"Label", "label"},
                                                              {"Thumbnail", "thumbnail"}};
    int checked = 0;
    for (const auto& item : manifest["bundles"].items()) {
        const std::string& rel = item.key();
        if (rel.find("2401582-38") != std::string::npos) continue;
        SCOPED_TRACE(rel);
        const std::string path = TestTools::getTestImagePath("mirax", rel);
        SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
        slideio::MRXSImageDriver driver;
        const auto slide = driver.openFile(path);
        const json& expected = item.value()["associated"];
        EXPECT_EQ(expected.size(), slide->getAuxImageNames().size());
        for (const std::string& name : slide->getAuxImageNames()) {
            const json& size = expected[openslideName.at(name)];
            EXPECT_EQ(cv::Rect(0, 0, size[0].get<int>(), size[1].get<int>()), slide->getAuxImage(name)->getRect())
                << name;
        }
        ++checked;
    }
    EXPECT_EQ(8, checked);
}

TEST(MRXSCorpusGeometry, aMaskAndHardwareBundleRendersItsPrimaryPyramidOnly)
{
    // The Zenodo slide carries a parallel mask hierarchy and a hardware-coordinate map.
    // Neither is applied (design section 2): positions are nominal, and both are named
    // as ignored in the metadata rather than silently dropped.
    const std::string path = TestTools::getTestImagePath("mirax/zenodo/20240920152150628", "20240920152150628.mrxs");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    slideio::MRXSImageDriver driver;
    const auto slide = driver.openFile(path);
    const slideio::Metadata& mrxs = slide->getMetadata()["mrxs"];
    EXPECT_EQ("nominal positions", mrxs["positionSource"].asString());
    std::vector<std::string> ignored;
    for (size_t i = 0; i < mrxs["ignoredLayers"].size(); ++i) {
        ignored.push_back(mrxs["ignoredLayers"][i].asString());
    }
    EXPECT_NE(ignored.end(), std::find(ignored.begin(), ignored.end(), "Slide zoom mask level2"));
    EXPECT_NE(ignored.end(), std::find(ignored.begin(), ignored.end(), "StitchingLayerForHWCoord"));
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

    // Open as a reader would, then a cold and a warm 1024 x 1024 read at level 0 centred on
    // a drawable (not on the bounds' middle, which can be a hole). Only pyramid tiles are
    // read: no associated image, and so no label, is decoded.
    const auto t3 = clock::now();
    const auto dataset = MRXSDataset::open(path);
    const auto t4 = clock::now();
    slideio::MRXSScene scene(dataset);
    const LevelGeometry& l0 = dataset->geometry.levels[0];
    const cv::Rect2d& b = *dataset->geometry.bounds0;
    const Drawable* centre = &l0.drawables()[0];
    double best = 1e300;
    for (const Drawable& d : l0.drawables()) {
        const double dx = d.x - (b.x + b.width / 2), dy = d.y - (b.y + b.height / 2);
        if (dx * dx + dy * dy < best) { best = dx * dx + dy * dy; centre = &d; }
    }
    const cv::Rect roi(int(centre->x) - 512, int(centre->y) - 512, 1024, 1024);
    cv::Mat pixels;
    const auto t5 = clock::now();
    scene.readBlock(roi, pixels);
    const auto t6 = clock::now();
    const RuntimeStats cold = dataset->runtime->stats();
    scene.readBlock(roi, pixels);
    const auto t7 = clock::now();
    const RuntimeStats warm = dataset->runtime->stats();
    std::cout << "open " << ms(t3, t4) << " ms; 1024x1024 level-0 read: cold " << ms(t5, t6) << " ms ("
              << cold.decodes << " decodes, " << cold.fileOpens << " files opened), warm " << ms(t6, t7) << " ms ("
              << warm.decodes - cold.decodes << " decodes, " << warm.hits - cold.hits << " hits); cache "
              << warm.cachedBytes / (1 << 20) << " MiB of " << (RuntimeOptions().decodedCacheBytes >> 20) << std::endl;
    EXPECT_LE(warm.cachedBytes, RuntimeOptions().decodedCacheBytes);
}
