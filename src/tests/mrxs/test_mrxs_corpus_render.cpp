// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// Rendering of the real integration bundles against the OpenSlide 4.0.1 reference regions
// written by auxfiles/mrxs-openslide-reference.py, with the tolerances fixed in design
// section 9 before this test existed.
#include <gtest/gtest.h>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <opencv2/core.hpp>
#include "slideio/drivers/mrxs/mrxsimagedriver.hpp"
#include "slideio/drivers/mrxs/mrxsdataset.hpp"
#include "slideio/drivers/mrxs/mrxsdecoder.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "tests/testlib/testtools.hpp"

using json = nlohmann::json;

namespace
{
    struct Stats
    {
        int maxDiff = 0;
        double mean = 0;
        double offBy2 = 0;      // share of pixels with any channel off by more than 2
    };

    Stats compare(const cv::Mat& got, const cv::Mat& expected)
    {
        cv::Mat diff;
        cv::absdiff(got, expected, diff);
        Stats s;
        double maxValue = 0;
        cv::minMaxLoc(diff.reshape(1), nullptr, &maxValue);
        s.maxDiff = static_cast<int>(maxValue);
        s.mean = cv::mean(diff.reshape(1))[0];
        cv::Mat channelMax;
        std::vector<cv::Mat> planes;
        cv::split(diff, planes);
        channelMax = cv::max(cv::max(planes[0], planes[1]), planes[2]);
        s.offBy2 = double(cv::countNonZero(channelMax > 2)) / double(channelMax.total());
        return s;
    }
}

// Half of the decoder measurement (design section 9): writes slideio's decode of every JPEG
// tile a reference region uses, so auxfiles/mrxs-decoder-difference.py can compare them
// with Pillow's (libjpeg-turbo, as OpenSlide uses). Run by hand with MRXS_TILE_DUMP_DIR set
// and --gtest_also_run_disabled_tests.
TEST(MRXSCorpusRender, DISABLED_dumpDecodedTilesOfEveryRegion)
{
    const char* dumpDir = std::getenv("MRXS_TILE_DUMP_DIR");
    if (!dumpDir) {
        GTEST_SKIP() << "MRXS_TILE_DUMP_DIR is not set";
    }
    const std::string manifestPath = TestTools::getTestImagePath("mirax/reference", "manifest.json");
    std::ifstream in(manifestPath);
    const json manifest = json::parse(in);
    json regionsOut;
    for (const auto& item : manifest["bundles"].items()) {
        const std::string& rel = item.key();
        if (rel.find("2401582-38") != std::string::npos) continue;
        const auto dataset = slideio::mrxs::MRXSDataset::open(TestTools::getTestImagePath("mirax", rel));
        for (const json& region : item.value()["regions"]) {
            const int level = region["level"].get<int>();
            if (dataset->structure.metadata.levels[level].format != slideio::mrxs::ImageFormat::Jpeg) continue;
            const auto& lg = dataset->geometry.levels[level];
            const double x = region["x"].get<double>(), y = region["y"].get<double>();
            json tiles = json::array();
            std::set<uint32_t> seen;
            for (uint32_t i : lg.query(x, y, x + region["width"].get<double>(), y + region["height"].get<double>())) {
                const uint32_t record = lg.drawables()[i].record;
                if (!seen.insert(record).second) continue;
                const auto& ref = dataset->structure.levels[level][record].payload;
                const auto& meta = dataset->structure.metadata.levels[level];
                // Unique across bundles: file numbers and offsets repeat from one bundle to the next.
                const std::string name = dataset->structure.paths.sceneName + "_" + std::to_string(ref.fileNumber)
                    + "_" + std::to_string(ref.offset) + ".rgb";
                const cv::Mat rgb = slideio::mrxs::decodeImage(slideio::mrxs::readPayload(dataset->structure, ref),
                                                               meta.format, meta.imageWidth, meta.imageHeight, name);
                std::ofstream out(std::string(dumpDir) + "/" + name, std::ios::binary);
                out.write(reinterpret_cast<const char*>(rgb.data), static_cast<std::streamsize>(rgb.total() * 3));
                tiles.push_back({{"file", ref.fileNumber}, {"offset", ref.offset}, {"length", ref.length},
                                 {"width", meta.imageWidth}, {"height", meta.imageHeight}, {"dump", name}});
            }
            regionsOut[region["file"].get<std::string>()] = {{"bundle", rel}, {"tiles", tiles}};
        }
    }
    std::ofstream(std::string(dumpDir) + "/regions.json") << regionsOut.dump(1);
}

TEST(MRXSCorpusRender, regionsMatchOpenSlideWithinTheDesignTolerances)
{
    const std::string manifestPath = TestTools::getTestImagePath("mirax/reference", "manifest.json");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(manifestPath);
    std::ifstream in(manifestPath);
    const json manifest = json::parse(in);
    // The decoder-only difference per region, measured by auxfiles/mrxs-decoder-difference.py:
    // slideio decodes JPEG with libjpeg 9f, OpenSlide with libjpeg-turbo, and they reconstruct
    // subsampled chroma differently. It is added to the tolerances, never guessed.
    const std::string decoderPath = TestTools::getTestImagePath("mirax/reference", "decoder-difference.json");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(decoderPath);
    std::ifstream decoderIn(decoderPath);
    const json decoderDifference = json::parse(decoderIn)["regions"];
    int regions = 0;
    for (const auto& item : manifest["bundles"].items()) {
        const std::string& rel = item.key();
        if (rel.find("2401582-38") != std::string::npos) {
            continue;   // five-channel: rejected as unsupported
        }
        SCOPED_TRACE(rel);
        const std::string path = TestTools::getTestImagePath("mirax", rel);
        SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
        slideio::MRXSImageDriver driver;
        const auto scene = driver.openFile(path)->getScene(0);
        const auto dataset = slideio::mrxs::MRXSDataset::open(path);
        // Zero-overlap bundles have only integer placement: the prototype was exact on every level.
        const bool integerPlacement = rel.find("20240920152150628") != std::string::npos;
        for (const json& region : item.value()["regions"]) {
            const int level = region["level"].get<int>();
            const cv::Rect rect(region["x"].get<int>(), region["y"].get<int>(),
                                region["width"].get<int>(), region["height"].get<int>());
            // A region with nothing in it would match on background alone and prove nothing.
            EXPECT_FALSE(dataset->geometry.levels[level].query(rect.x, rect.y, rect.br().x, rect.br().y).empty())
                << region["file"] << " contains no drawable";
            const std::string file = region["file"].get<std::string>();
            const int decoder = decoderDifference.contains(file) ? decoderDifference[file].get<int>() : 0;
            cv::Mat got;
            scene->readResampledLevelBlockChannels(level, rect, rect.size(), {}, got);
            cv::Mat expected;
            slideio::ImageTools::readSmallImageRaster(
                TestTools::getTestImagePath("mirax/reference", region["file"].get<std::string>()), expected);
            ASSERT_EQ(expected.size(), got.size());
            ASSERT_EQ(expected.type(), got.type());
            const Stats s = compare(got, expected);
            const bool exactClass = level == 0 || integerPlacement;
            std::cout << std::left << std::setw(34) << rel.substr(rel.find('/') + 1, 33) << " L" << level
                      << (exactClass ? " exact " : " frac  ") << " max " << s.maxDiff << " mean "
                      << std::setprecision(3) << s.mean << " off>2 " << s.offBy2 * 100 << "% decoder "
                      << decoder << std::endl;
            if (exactClass) {
                EXPECT_LE(s.maxDiff, decoder) << file;
            }
            else {
                EXPECT_LE(s.maxDiff, 6 + decoder) << file;
                EXPECT_LE(s.mean, 0.5) << file;
                // Where the decoders differ by more than 2 the share of pixels off by more
                // than 2 measures the codecs, not the renderer.
                if (decoder <= 2) {
                    EXPECT_LE(s.offBy2, 0.005) << file;
                }
            }
            ++regions;
        }
    }
    EXPECT_EQ(148, regions);
}
