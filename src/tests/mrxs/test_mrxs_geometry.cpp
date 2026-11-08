// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// Camera positions and drawable geometry (design section 5), checked against the
// independently computed geometry in every fixture's .expected.json, which the
// reference harness has in turn checked against OpenSlide 4.0.1.
#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <random>
#include <string>
#include <tuple>
#include <vector>
#include <zlib.h>
#include "slideio/drivers/mrxs/mrxsgeometry.hpp"
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

    std::vector<std::string> fixtureNames()
    {
        const json manifest = load(dataDir() / "manifest.json");
        std::vector<std::string> names;
        for (const auto& name : manifest["fixtures"]) {
            names.push_back(name.get<std::string>());
        }
        return names;
    }

    std::string entry(const std::string& name) { return (dataDir() / (name + ".mrxs")).u8string(); }

    Geometry geometryOf(const std::string& name) { return Geometry::build(readSlideStructure(entry(name))); }

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

    // (grid row, grid column, image index, subtile, source, destination), comparable exactly.
    using DrawableKey = std::tuple<int, int, int64_t, int, int, double, double, double, double, double, double>;

    std::vector<DrawableKey> expectedDrawables(const json& level, int nx)
    {
        std::vector<DrawableKey> keys;
        for (const json& d : level["drawables"]) {
            keys.emplace_back(d["grid"][0].get<int>(), d["grid"][1].get<int>(),
                              d["image"][1].get<int64_t>() * nx + d["image"][0].get<int64_t>(),
                              d["subtile"][0].get<int>(), d["subtile"][1].get<int>(),
                              d["source"][0].get<double>(), d["source"][1].get<double>(),
                              d["destination"][0].get<double>(), d["destination"][1].get<double>(),
                              d["source"][2].get<double>(), d["source"][3].get<double>());
        }
        std::sort(keys.begin(), keys.end());
        return keys;
    }

    std::vector<DrawableKey> actualDrawables(const SlideStructure& slide, const LevelGeometry& level, size_t l)
    {
        std::vector<DrawableKey> keys;
        for (const Drawable& d : level.drawables()) {
            keys.emplace_back(d.gridRow, d.gridColumn, slide.levels[l][d.record].imageIndex, d.xi, d.yi,
                              d.xi * level.tileWidth, d.yi * level.tileHeight, d.x, d.y,
                              level.tileWidth, level.tileHeight);
        }
        std::sort(keys.begin(), keys.end());
        return keys;
    }

    std::vector<uint8_t> packPositions(const std::vector<std::tuple<int, int, int>>& records)
    {
        std::vector<uint8_t> out;
        for (const auto& [flag, x, y] : records) {
            out.push_back(static_cast<uint8_t>(flag));
            for (int32_t v : {x, y}) {
                uint32_t u;
                std::memcpy(&u, &v, 4);
                for (int b = 0; b < 4; ++b) out.push_back(static_cast<uint8_t>(u >> (8 * b)));
            }
        }
        return out;
    }

    std::vector<uint8_t> deflate(const std::vector<uint8_t>& raw)
    {
        uLongf size = compressBound(static_cast<uLong>(raw.size()));
        std::vector<uint8_t> out(size);
        compress2(out.data(), &size, raw.data(), static_cast<uLong>(raw.size()), 9);
        out.resize(size);
        return out;
    }
}

TEST(MRXSGeometry, everyValidFixtureHasExactlyItsGeometry)
{
    int checked = 0;
    for (const std::string& name : fixtureNames()) {
        const json expected = load(dataDir() / (name + ".expected.json"));
        if (expected.contains("error")) {
            continue;
        }
        SCOPED_TRACE(name);
        const SlideStructure slide = readSlideStructure(entry(name));
        const Geometry geometry = Geometry::build(slide);
        EXPECT_EQ(expected["position_aware"].get<bool>(), geometry.positionAware);
        EXPECT_EQ(expected["nominal_lattice"][0].get<int64_t>(), geometry.latticeWidth);
        EXPECT_EQ(expected["nominal_lattice"][1].get<int64_t>(), geometry.latticeHeight);
        EXPECT_EQ(expected["camera_grid"][0].get<int>(), geometry.cameraColumns);
        EXPECT_EQ(expected["camera_grid"][1].get<int>(), geometry.cameraRows);
        const json& levels = expected["levels"];
        ASSERT_EQ(levels.size(), geometry.levels.size());
        for (size_t l = 0; l < levels.size(); ++l) {
            SCOPED_TRACE("level " + std::to_string(l));
            const LevelGeometry& level = geometry.levels[l];
            EXPECT_EQ(levels[l]["width"].get<int64_t>(), level.width);
            EXPECT_EQ(levels[l]["height"].get<int64_t>(), level.height);
            EXPECT_EQ(levels[l]["downsample"].get<int64_t>(), level.downsample);
            EXPECT_EQ(levels[l]["concat"].get<int64_t>(), level.concat);
            EXPECT_EQ(expectedDrawables(levels[l], slide.metadata.nx), actualDrawables(slide, level, l));
        }
        const json& bounds = expected["bounds_level0"];
        ASSERT_TRUE(geometry.bounds0.has_value());
        EXPECT_DOUBLE_EQ(bounds[0].get<double>(), geometry.bounds0->x);
        EXPECT_DOUBLE_EQ(bounds[1].get<double>(), geometry.bounds0->y);
        EXPECT_DOUBLE_EQ(bounds[2].get<double>(), geometry.bounds0->width);
        EXPECT_DOUBLE_EQ(bounds[3].get<double>(), geometry.bounds0->height);
        ++checked;
    }
    EXPECT_GE(checked, 9);
}

TEST(MRXSGeometry, badPositionMapsAreCorrupt)
{
    for (const std::string name : {"bad-map-length", "bad-map-flag", "bad-zlib-map"}) {
        SCOPED_TRACE(name);
        const SlideStructure slide = readSlideStructure(entry(name));
        const std::string message = errorOf([&] { Geometry::build(slide); });
        EXPECT_EQ(0u, message.find("MRXS corrupt:")) << message;
    }
}

TEST(MRXSGeometry, decodesARawMapAndScalesItByC0)
{
    const auto raw = packPositions({{0, 10, -20}, {1, 30, 40}});
    const auto positions = decodePositionMap(raw, PositionSource::Raw, 2, 1, 4);
    ASSERT_EQ(2u, positions.size());
    EXPECT_EQ(40, positions[0].x);
    EXPECT_EQ(-80, positions[0].y);
    EXPECT_EQ(120, positions[1].x);
    EXPECT_EQ(160, positions[1].y);
}

TEST(MRXSGeometry, decodesAZlibMap)
{
    const auto raw = packPositions({{0, 1, 2}, {0, 3, 4}, {1, -5, 6}});
    const auto positions = decodePositionMap(deflate(raw), PositionSource::Zlib, 3, 1, 1);
    ASSERT_EQ(3u, positions.size());
    EXPECT_EQ(-5, positions[2].x);
    EXPECT_EQ(6, positions[2].y);
}

TEST(MRXSGeometry, aZlibMapThatInflatesTooFarIsCorrupt)
{
    // One record more than the grid has: the inflated size must be exact, and the
    // decoder must stop rather than allocate whatever the stream produces.
    const auto raw = packPositions({{0, 1, 2}, {0, 3, 4}, {0, 5, 6}});
    const std::string message = errorOf([&] { decodePositionMap(deflate(raw), PositionSource::Zlib, 2, 1, 1); });
    EXPECT_EQ(0u, message.find("MRXS corrupt:")) << message;
}

TEST(MRXSGeometry, aZlibMapThatInflatesShortIsCorrupt)
{
    const auto raw = packPositions({{0, 1, 2}});
    const std::string message = errorOf([&] { decodePositionMap(deflate(raw), PositionSource::Zlib, 2, 1, 1); });
    EXPECT_EQ(0u, message.find("MRXS corrupt:")) << message;
}

TEST(MRXSGeometry, queriesReturnDescendingGridOrder)
{
    const Geometry geometry = geometryOf("rawmap-d2");
    for (const LevelGeometry& level : geometry.levels) {
        const auto hits = level.query(-1e9, -1e9, 1e9, 1e9);
        ASSERT_EQ(level.drawables().size(), hits.size());
        for (size_t i = 1; i < hits.size(); ++i) {
            const Drawable& a = level.drawables()[hits[i - 1]];
            const Drawable& b = level.drawables()[hits[i]];
            EXPECT_TRUE(std::make_pair(a.gridRow, a.gridColumn) > std::make_pair(b.gridRow, b.gridColumn));
        }
    }
}

TEST(MRXSGeometry, queriesMatchABruteForceScan)
{
    // The spatial index is an optimisation; it must return exactly the drawables a full
    // scan finds with the same "can contribute to this region" predicate.
    std::mt19937 random(20261107);
    for (const std::string name : {"rawmap-d2", "saved-c0-4", "nomap-overlap-d2", "nomap-d1", "layout-variants"}) {
        SCOPED_TRACE(name);
        const Geometry geometry = geometryOf(name);
        for (const LevelGeometry& level : geometry.levels) {
            std::uniform_real_distribution<double> px(-5.0, level.width + 5.0), py(-5.0, level.height + 5.0);
            std::uniform_real_distribution<double> sw(1.0, level.width / 3.0 + 2.0), sh(1.0, level.height / 3.0 + 2.0);
            for (int trial = 0; trial < 50; ++trial) {
                // Non-empty regions only: an empty region has no pixels for anything to touch.
                const double x0 = std::floor(px(random)), y0 = std::floor(py(random));
                const double x1 = x0 + std::floor(sw(random)), y1 = y0 + std::floor(sh(random));
                std::vector<uint32_t> expected;
                for (uint32_t i = 0; i < level.drawables().size(); ++i) {
                    if (level.contributes(level.drawables()[i], x0, y0, x1, y1)) expected.push_back(i);
                }
                std::vector<uint32_t> got = level.query(x0, y0, x1, y1);
                std::sort(expected.begin(), expected.end());
                std::sort(got.begin(), got.end());
                EXPECT_EQ(expected, got) << "region " << x0 << "," << y0 << " - " << x1 << "," << y1;
            }
        }
    }
}

TEST(MRXSGeometry, aFarCameraDoesNotInflateTheSpatialIndex)
{
    // Positions come from the map unchecked, so one camera far from the rest must not make
    // the index cover the empty space between them with dense bins. nomap-d1: 6 x 4 cameras
    // of 8 x 8, built as if from a map with the camera of the last stored image moved away.
    const SlideStructure slide = readSlideStructure(entry("nomap-d1"));
    const SlideMetadata& meta = slide.metadata;
    const ImageRecord& moved = slide.levels[0].back();
    const size_t movedCamera = size_t(moved.y / meta.d) * (meta.nx / meta.d) + size_t(moved.x / meta.d);
    for (const int64_t far : {int64_t(1) << 18, int64_t(1) << 40}) {
        SCOPED_TRACE(far);
        std::vector<CameraPosition> positions;
        for (int y = 0; y < meta.ny; ++y) {
            for (int x = 0; x < meta.nx; ++x) {
                positions.push_back({int64_t(x) * meta.levels[0].imageWidth, int64_t(y) * meta.levels[0].imageHeight});
            }
        }
        positions[movedCamera] = {far, far};
        const Geometry geometry = Geometry::build(slide, positions, true);
        for (const LevelGeometry& level : geometry.levels) {
            ASSERT_LE(level.memoryBytes(), size_t(1) << 20) << "level concat " << level.concat;
            for (const Drawable& d : level.drawables()) {
                const double x0 = std::floor(d.x), y0 = std::floor(d.y);
                std::vector<uint32_t> expected;
                for (uint32_t i = 0; i < level.drawables().size(); ++i) {
                    if (level.contributes(level.drawables()[i], x0, y0, x0 + 3, y0 + 3)) expected.push_back(i);
                }
                std::vector<uint32_t> got = level.query(x0, y0, x0 + 3, y0 + 3);
                std::sort(got.begin(), got.end());
                EXPECT_EQ(expected, got) << "region at " << x0 << "," << y0;
            }
        }
    }
}

TEST(MRXSGeometry, aLowResolutionLevelNeedsTheLevelZeroCameras)
{
    // Active-camera filtering is seeded at level 0 even when only a coarse level is used:
    // the inactive camera with garbage coordinates in rawmap-d2 must not appear anywhere.
    const Geometry geometry = geometryOf("rawmap-d2");
    for (const LevelGeometry& level : geometry.levels) {
        for (const Drawable& d : level.drawables()) {
            EXPECT_LT(d.x, 700.0 / level.concat);
        }
    }
}
