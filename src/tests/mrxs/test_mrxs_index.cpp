// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// Metadata and bounded index reading (design section 4), checked against every
// committed fixture's .expected.json: valid fixtures must enumerate exactly the
// records the generator wrote; invalid ones must fail on open with their category.
#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>
#include "slideio/drivers/mrxs/mrxsindex.hpp"
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
        std::vector<std::string> names;
        // Held in a variable: iterating load(...)["fixtures"] directly would read a
        // reference into a temporary destroyed before the loop body runs.
        const json manifest = load(dataDir() / "manifest.json");
        for (const auto& name : manifest["fixtures"]) {
            names.push_back(name.get<std::string>());
        }
        return names;
    }

    std::string entry(const std::string& name) { return (dataDir() / (name + ".mrxs")).u8string(); }

    // Position-map length is checked when positions are decoded (step 6), not here.
    bool failsWhileIndexing(const json& expected)
    {
        return expected.contains("error") && expected["error_on"] == "open" && expected["name"] != "bad-map-length";
    }

    ImageFormat formatOf(const std::string& name)
    {
        if (name == "JPEG") return ImageFormat::Jpeg;
        if (name == "PNG") return ImageFormat::Png;
        return ImageFormat::Bmp24;
    }
}

TEST(MRXSIndex, everyValidFixtureEnumeratesExactlyItsRecords)
{
    int checked = 0;
    for (const std::string& name : fixtureNames()) {
        const json expected = load(dataDir() / (name + ".expected.json"));
        if (expected.contains("error")) {
            continue;
        }
        SCOPED_TRACE(name);
        const SlideStructure slide = readSlideStructure(entry(name));
        EXPECT_EQ(expected["slide_id"].get<std::string>(), slide.metadata.slideId);
        EXPECT_EQ(expected["image_grid"][0].get<int>(), slide.metadata.nx);
        EXPECT_EQ(expected["image_grid"][1].get<int>(), slide.metadata.ny);
        EXPECT_EQ(expected["image_grid"][2].get<int>(), slide.metadata.d);
        EXPECT_EQ(expected["data_files"].get<size_t>(), slide.metadata.dataFiles.size());
        const json& levels = expected["levels"];
        ASSERT_EQ(levels.size(), slide.metadata.levels.size());
        ASSERT_EQ(levels.size(), slide.levels.size());
        for (size_t l = 0; l < levels.size(); ++l) {
            SCOPED_TRACE("level " + std::to_string(l));
            const LevelMetadata& meta = slide.metadata.levels[l];
            EXPECT_EQ(levels[l]["concat"].get<int64_t>(), meta.concat);
            EXPECT_EQ(levels[l]["image_size"][0].get<int>(), meta.imageWidth);
            EXPECT_EQ(levels[l]["image_size"][1].get<int>(), meta.imageHeight);
            EXPECT_EQ(formatOf(levels[l]["format"].get<std::string>()), meta.format);
            std::vector<std::vector<int64_t>> got;
            for (const ImageRecord& r : slide.levels[l]) {
                EXPECT_EQ(r.imageIndex % slide.metadata.nx, r.x);
                EXPECT_EQ(r.imageIndex / slide.metadata.nx, r.y);
                got.push_back({r.imageIndex, r.payload.fileNumber, r.payload.offset, r.payload.length});
            }
            std::sort(got.begin(), got.end());
            EXPECT_EQ(levels[l]["records"].get<std::vector<std::vector<int64_t>>>(), got);
        }
        const json& positions = expected["positions"];
        if (positions.is_null()) {
            EXPECT_EQ(PositionSource::None, slide.positionSource);
        }
        else {
            EXPECT_EQ(positions == "raw" ? PositionSource::Raw : PositionSource::Zlib, slide.positionSource);
            EXPECT_GT(slide.positions.length, 0);
        }
        std::set<std::string> associated;
        for (const auto& item : slide.associated) {
            associated.insert(item.first);
            EXPECT_GT(item.second.length, 0) << item.first;
        }
        std::set<std::string> expectedAssociated;
        for (const auto& item : expected["associated"].items()) {
            expectedAssociated.insert(item.key());
        }
        EXPECT_EQ(expectedAssociated, associated);
        ++checked;
    }
    EXPECT_GE(checked, 9);
}

TEST(MRXSIndex, everyInvalidFixtureFailsWithItsCategory)
{
    int checked = 0;
    for (const std::string& name : fixtureNames()) {
        const json expected = load(dataDir() / (name + ".expected.json"));
        if (!failsWhileIndexing(expected)) {
            continue;
        }
        const std::string tag = "MRXS " + expected["error"].get<std::string>() + ":";
        std::string message;
        try {
            readSlideStructure(entry(name));
        }
        catch (const slideio::RuntimeError& e) {
            message = e.what();
        }
        EXPECT_EQ(0u, message.find(tag)) << name << " (" << expected["reason"].get<std::string>()
                                         << ") should fail with '" << tag << "', got: '" << message << "'";
        ++checked;
    }
    EXPECT_GE(checked, 10);
}

TEST(MRXSIndex, fixturesThatFailOnlyWhenReadOpen)
{
    for (const std::string& name : {"codec-mismatch", "bmp-not-24bit"}) {
        SCOPED_TRACE(name);
        EXPECT_NO_THROW(readSlideStructure(entry(name)));
    }
}

TEST(MRXSIndex, pagesAreFollowedByPointerNotByFilePosition)
{
    // The chain is written out of file order; OpenSlide misreads it, the format
    // description and this driver follow the next pointers.
    const SlideStructure slide = readSlideStructure(entry("pages-out-of-order"));
    const json expected = load(dataDir() / "pages-out-of-order.expected.json");
    EXPECT_EQ(expected["levels"][0]["records"].size(), slide.levels[0].size());
}

TEST(MRXSIndex, aBudgetTooSmallForThePagesIsALimitError)
{
    IndexBudget budget;
    budget.maxPages = 1;
    std::string message;
    try {
        readSlideStructure(entry("layout-variants"), budget);
    }
    catch (const slideio::RuntimeError& e) {
        message = e.what();
    }
    EXPECT_EQ(0u, message.find("MRXS limit:")) << message;
}

TEST(MRXSIndex, metadataCarriesPerLevelGeometryInputs)
{
    const SlideStructure slide = readSlideStructure(entry("rawmap-d2"));
    const json expected = load(dataDir() / "rawmap-d2.expected.json");
    ASSERT_EQ(4u, slide.metadata.levels.size());
    EXPECT_DOUBLE_EQ(2.0, slide.metadata.levels[0].overlapX);
    EXPECT_DOUBLE_EQ(0.25, slide.metadata.levels[3].overlapY);
    EXPECT_DOUBLE_EQ(expected["mpp_um"][0].get<double>(), slide.metadata.levels[0].mppX);
    EXPECT_EQ(0x00F0F0F0, slide.metadata.levels[0].fillColorBGR);
    ASSERT_TRUE(slide.metadata.objective.has_value());
    EXPECT_DOUBLE_EQ(expected["objective"].get<double>(), *slide.metadata.objective);
}
