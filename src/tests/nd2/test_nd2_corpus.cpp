// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// The ND2 driver against the 64 corpus files under $SLIDEIO_IMAGES_PATH/nd2 (design
// §8.2). The references are committed in data/:
//   nd2-scenes.tsv   Bio-Formats series                 (auxfiles/nd2-fixtures/Nd2Scenes.java)
//   nd2-regions.tsv  Bio-Formats 7x5 regions, CRC-32    (auxfiles/nd2-fixtures/make_region_reference.py)
//   nd2-loops.tsv    validation inventory loops         (the same script)
//   nd2-times.tsv    nd2 frame times of two time-lapses (the same script)
// Every suite here is named ND2Corpus*; CI excludes them. A test driven by a reference
// table checks every file of it that is present; with SLIDEIO_SKIP_MISSING_IMAGES set an
// absent file is named and skipped, and the totals are only asserted when none is.
#include <gtest/gtest.h>
#include <zlib.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include "slideio/drivers/nd2/nd2imagedriver.hpp"
#include "slideio/drivers/nd2/nd2slide.hpp"
#include "slideio/drivers/nd2/nd2chunkfile.hpp"
#include "slideio/drivers/nd2/nd2experiment.hpp"
#include "slideio/drivers/nd2/nd2variant.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/core/tools/filereader.hpp"
#include "slideio/core/tools/tiledareadownscale.hpp"
#include "tests/testlib/testtools.hpp"

using namespace slideio;

namespace
{
    const char* const kVpa002 = "zenodo/bioformats-issue/2D 500uM VPA002.nd2";
    const char* const kStaleMap = "aryeh/b16_pdtB+y50__crop.nd2";

    std::string corpusPath(const std::string& rel)
    {
        return TestTools::getTestImagePath("nd2", rel);
    }

    // With skipping on, an absent corpus file is reported and left out; with it off it is
    // checked like any other, so it fails.
    bool absent(const std::string& rel, std::vector<std::string>& skipped)
    {
        if (TestTools::skipsMissingImages() && !TestTools::imageExists(corpusPath(rel))) {
            skipped.push_back(rel);
            return true;
        }
        return false;
    }

    void reportSkipped(const std::vector<std::string>& skipped)
    {
        for (const std::string& rel : skipped) {
            std::cout << "  absent, skipped: " << rel << std::endl;
        }
    }

// Ends a table-driven test as skipped when files were left out, after the present ones
// have been checked (a failure among them still fails the test). A macro, because
// GTEST_SKIP returns from the function it is in.
#define ND2_SKIP_IF_ANY_ABSENT(skipped)                                                        do {                                                                                           if (!(skipped).empty()) {                                                                      GTEST_SKIP() << (skipped).size() << " corpus file(s) absent; the rest checked";         }                                                                                      } while (false)

    using Row = std::map<std::string, std::string>;

    // A tab-separated reference with a header row.
    std::vector<Row> readTsv(const std::string& name)
    {
        const std::string path = std::string(ND2_TEST_DATA_DIR) + "/" + name;
        std::ifstream in(path, std::ios::binary);
        EXPECT_TRUE(in.good()) << "cannot open " << path;
        const auto split = [](const std::string& line) {
            std::vector<std::string> fields;
            std::string field;
            std::istringstream ss(line);
            while (std::getline(ss, field, '\t')) {
                fields.push_back(field);
            }
            if (!line.empty() && line.back() == '\t') {
                fields.emplace_back();
            }
            return fields;
        };
        std::string line;
        std::getline(in, line);
        const std::vector<std::string> header = split(line);
        std::vector<Row> rows;
        while (std::getline(in, line)) {
            if (line.empty()) {
                continue;
            }
            const std::vector<std::string> fields = split(line);
            EXPECT_EQ(header.size(), fields.size()) << path << ": " << line;
            Row row;
            for (size_t i = 0; i < header.size() && i < fields.size(); ++i) {
                row[header[i]] = fields[i];
            }
            rows.push_back(std::move(row));
        }
        return rows;
    }

    // Rows grouped by `file`, files in first-seen order.
    std::vector<std::pair<std::string, std::vector<Row>>> byFile(const std::vector<Row>& rows)
    {
        std::vector<std::pair<std::string, std::vector<Row>>> groups;
        for (const Row& row : rows) {
            if (groups.empty() || groups.back().first != row.at("file")) {
                groups.emplace_back(row.at("file"), std::vector<Row>());
            }
            groups.back().second.push_back(row);
        }
        return groups;
    }

    int toInt(const std::string& s) { return std::stoi(s); }

    std::shared_ptr<CVSlide> openCorpus(const std::string& rel)
    {
        ND2ImageDriver driver;
        return driver.openFile(corpusPath(rel));
    }

    DataType bioFormatsType(const std::string& name)
    {
        static const std::map<std::string, DataType> types = {
            {"uint8", DataType::DT_Byte}, {"uint16", DataType::DT_UInt16}, {"float", DataType::DT_Float32},
        };
        const auto it = types.find(name);
        EXPECT_NE(types.end(), it) << "unexpected Bio-Formats pixel type " << name;
        return it == types.end() ? DataType::DT_Unknown : it->second;
    }

    // A Bio-Formats value, or 0 for an empty field (none reported).
    double value(const std::string& s)
    {
        return s.empty() ? 0. : std::stod(s);
    }

    bool same(double driver, double bioFormats)
    {
        if (driver == 0. || bioFormats == 0.) {
            return driver == bioFormats;
        }
        return std::abs(driver - bioFormats) <= 1e-9 * std::abs(bioFormats);
    }

    std::string crcOf(const cv::Mat& raster)
    {
        const uLong crc = crc32(0L, raster.data, static_cast<uInt>(raster.total() * raster.elemSize()));
        char hex[9];
        std::snprintf(hex, sizeof(hex), "%08lx", static_cast<unsigned long>(crc));
        return hex;
    }

    // Reads every region row of one file; returns the number of mismatches (the first few
    // are reported) and adds the matches to `matches`. A row whose time point is beyond the
    // scene's T (a frame the driver ignores) is not read but counted in `beyondT`, per file.
    int checkRegions(const std::string& file, const std::vector<Row>& rows, int& matches,
                     std::map<std::string, int>& beyondT)
    {
        const auto slide = openCorpus(file);
        EXPECT_TRUE(slide) << file;
        if (!slide) {
            return static_cast<int>(rows.size());
        }
        int mismatches = 0;
        for (const Row& row : rows) {
            const int series = toInt(row.at("series"));
            const int z = toInt(row.at("z"));
            const int c = toInt(row.at("c"));
            const int t = toInt(row.at("t"));
            const cv::Rect rect(toInt(row.at("rx")), toInt(row.at("ry")), toInt(row.at("rw")), toInt(row.at("rh")));
            const auto scene = slide->getScene(series);
            if (t >= scene->getNumTFrames()) {
                ++beyondT[file];
                continue;
            }
            cv::Mat raster;
            scene->read4DBlockChannels(rect, {c}, cv::Range(z, z + 1), cv::Range(t, t + 1), raster);
            EXPECT_EQ(rect.width, raster.cols);
            EXPECT_EQ(rect.height, raster.rows);
            EXPECT_EQ(1, raster.channels());
            EXPECT_TRUE(raster.isContinuous());
            const std::string crc = crcOf(raster);
            if (row.at("crc32") == crc) {
                ++matches;
            }
            else if (++mismatches <= 10) {
                ADD_FAILURE() << file << " series " << series << " z " << z << " c " << c << " t " << t
                              << " at (" << rect.x << "," << rect.y << "): crc " << crc
                              << ", reference " << row.at("crc32");
            }
        }
        return mismatches;
    }

    std::string loopsText(const nd2::Experiment& experiment)
    {
        std::string text;
        for (const nd2::Loop& loop : experiment.loops()) {
            if (!text.empty()) {
                text += " ";
            }
            text += loop.axis == nd2::Axis::T ? "T" : loop.axis == nd2::Axis::P ? "P" : "Z";
            text += std::to_string(loop.count);
        }
        return text;
    }
}

// Scene list, geometry, type and calibration against Bio-Formats' series (one row per
// series in nd2-scenes.tsv; 290110.tranMgc005cr.nd2 is not in it, see
// InvalidAttributesRefused). Scenes map to series by index: both are the valid XY points
// in loop order (design §4.6).
//
// Recorded differences, asserted rather than skipped (design §2, §4.5, §6.2, §6.3):
// - VPA002 (4 channels x RGB): Bio-Formats reports 4 channels and reads them with the
//   wrong stride (FD §6.2); the driver exposes the 12 components.
// - T: equal everywhere but one file. With T the outermost loop the driver trims trailing
//   empty time points and adds surplus ones (FD 7.4) only while each is complete and its
//   times follow the series (R29, design 4.5). That gives Bio-Formats' T on all the
//   surplus files: old_cells_02 42, rfp_h2a_cells_02 116, jonas/3 27 extended;
//   190821_L1_IRS-Akt_CS2003 31 and multicolorTest 13 (partial surplus) and
//   130606_JCC719_003 20 (82 surplus frames whose times run back into the series) not.
// - b16_pdtB+y50__crop.nd2: T is 57, Bio-Formats 56 (R26). All 684 frames are present;
//   nd2 and the file's text (T'(57) x XY(12)) agree on 57. Bio-Formats
//   discards the stale chunk map, rescans, and ends with fewer than 684 offsets; its
//   image-count fallback then floors to 56 time points.
// - Calibration, kRecorded, every pair printed:
//   magnification  dObjectiveMag when > 0, else the number before the 'x' of the
//                  objective name (R27), as Bio-Formats does for chunked files. Bio-Formats
//                  reports none for the 8 named legacy files (it does not parse their
//                  names) nor for BF007 (100x) and 130606_JCC719_003 (60x), whose names nd2
//                  reads as the driver does.
//   tIncrement_s   each scene's measured mean step of its own plane times (its P, Z = 0),
//                  the nominal period only when that cannot be measured (R28, R30). The
//                  reference is the mean step of Bio-Formats' plane times of each series.
//                  They differ where Bio-Formats has no plane times (the 9 legacy files;
//                  T = 1, where the driver falls back to the period: wtf_DIC,
//                  wtf_alignment); on b16_pdtB, where T differs (above); and on series 1 of
//                  cag_p5_simgc_2511_70ms22s_crop, where Bio-Formats repeats series 0's
//                  plane times (599.998 s) while that position's frames step 599.952 s.
//   physX/Y_um     but3_cont200-1, but3_cont2002 and por003 (legacy): Bio-Formats reports
//                  no pixel size; the driver's 50 and 100 um are the calibration nd2 reports.
TEST(ND2CorpusOpen, ScenesMatchBioFormats)
{
    // file -> quantities that differ from Bio-Formats (design §6.3).
    const std::map<std::string, std::set<std::string>> kRecorded = {
        {"aryeh/4_2_1_cont_NoMR001.nd2", {"magnification", "tIncrement_s"}},
        {"aryeh/4_con_2_1_cot002.nd2", {"magnification", "tIncrement_s"}},
        {"aryeh/Time sequence 24.nd2", {"tIncrement_s"}},
        {"aryeh/b16_14_12.nd2", {"magnification", "tIncrement_s"}},
        {"aryeh/b16_pdtB+y50__crop.nd2", {"tIncrement_s"}},
        {"aryeh/but3_cont200-1.nd2", {"magnification", "physX_um", "physY_um", "tIncrement_s"}},
        {"aryeh/but3_cont2002.nd2", {"magnification", "physX_um", "physY_um", "tIncrement_s"}},
        {"aryeh/multipoint.nd2", {"magnification", "tIncrement_s"}},
        {"aryeh/por003.nd2", {"magnification", "physX_um", "physY_um", "tIncrement_s"}},
        {"aryeh/weekend002.nd2", {"magnification", "tIncrement_s"}},
        {"jonas/512c/cag_p5_simgc_2511_70ms22s_crop.nd2", {"tIncrement_s"}},
        {"jonas/512c/wtf_DIC.nd2", {"tIncrement_s"}},
        {"jonas/512c/wtf_alignment.nd2", {"tIncrement_s"}},
        {"jonas/qa-7534/130606_JCC719_003.nd2", {"magnification"}},
        {"maxime/BF007.nd2", {"magnification"}},
    };
    // file -> (Bio-Formats T, driver T)
    const std::map<std::string, std::pair<int, int>> kTimeDifferences = {
        {kStaleMap, {56, 57}},
    };
    std::vector<std::string> skipped;
    std::map<std::string, std::set<std::string>> recorded;
    size_t seriesChecked = 0;
    for (const auto& [file, rows] : byFile(readTsv("nd2-scenes.tsv"))) {
        if (absent(file, skipped)) {
            continue;
        }
        SCOPED_TRACE(file);
        const auto slide = openCorpus(file);
        ASSERT_TRUE(slide);
        ASSERT_EQ(static_cast<int>(rows.size()), slide->getNumScenes());
        for (size_t i = 0; i < rows.size(); ++i) {
            const Row& row = rows[i];
            const int series = toInt(row.at("series"));
            ASSERT_EQ(static_cast<int>(i), series);
            SCOPED_TRACE("series " + row.at("series"));
            const auto scene = slide->getScene(series);
            ASSERT_TRUE(scene);
            const cv::Rect rect = scene->getRect();
            EXPECT_EQ(toInt(row.at("width")), rect.width);
            EXPECT_EQ(toInt(row.at("height")), rect.height);
            if (file == kVpa002) {
                EXPECT_EQ(4, toInt(row.at("c")));
                EXPECT_EQ(12, scene->getNumChannels());
            }
            else {
                EXPECT_EQ(toInt(row.at("c")), scene->getNumChannels());
            }
            EXPECT_EQ(toInt(row.at("z")), scene->getNumZSlices());
            const auto timeDifference = kTimeDifferences.find(file);
            if (timeDifference != kTimeDifferences.end()) {
                EXPECT_EQ(timeDifference->second.first, toInt(row.at("t")));
                EXPECT_EQ(timeDifference->second.second, scene->getNumTFrames());
            }
            else {
                EXPECT_EQ(toInt(row.at("t")), scene->getNumTFrames());
            }
            const DataType type = bioFormatsType(row.at("pixelType"));
            for (int c = 0; c < scene->getNumChannels(); ++c) {
                EXPECT_EQ(type, scene->getChannelDataType(c)) << "channel " << c;
            }

            const Resolution res = scene->getResolution();
            const std::pair<const char*, double> quantities[] = {
                {"physX_um", res.x * 1e6},
                {"physY_um", res.y * 1e6},
                {"physZ_um", scene->getZSliceResolution() * 1e6},
                {"tIncrement_s", scene->getTFrameResolution()},
                {"magnification", scene->getMagnification()},
            };
            for (const auto& [column, driver] : quantities) {
                const double reference = value(row.at(column));
                if (same(driver, reference)) {
                    continue;
                }
                std::printf("difference %s %s series %d: driver %.17g, Bio-Formats %.17g\n",
                            column, file.c_str(), series, driver, reference);
                recorded[file].insert(column);
            }
            ++seriesChecked;
        }
    }
    reportSkipped(skipped);
    for (const auto& [file, columns] : recorded) {
        std::cout << "recorded: " << file << ":";
        for (const std::string& column : columns) {
            std::cout << " " << column;
        }
        std::cout << std::endl;
    }
    if (skipped.empty()) {
        EXPECT_EQ(248u, seriesChecked);
        EXPECT_EQ(kRecorded, recorded);
    }
    else {
        for (const auto& [file, columns] : recorded) {
            const auto it = kRecorded.find(file);
            EXPECT_TRUE(it != kRecorded.end() && it->second == columns) << file;
        }
    }
    ND2_SKIP_IF_ANY_ABSENT(skipped);
}

// uiWidth 0 (FD 14.3): refused, naming the field.
TEST(ND2CorpusOpen, InvalidAttributesRefused)
{
    const std::string path = TestTools::getTestImagePath("nd2", "jonas/divisionByZero/290110.tranMgc005cr.nd2");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    ND2ImageDriver driver;
    try {
        driver.openFile(path);
        FAIL() << "the file opened";
    }
    catch (const std::exception& e) {
        EXPECT_NE(std::string::npos, std::string(e.what()).find("uiWidth")) << e.what();
    }
}

// The flattened loops of every chunked file, before T trimming, against the validation
// inventory (nd2spec, which agrees with Bio-Formats on all of them; inventory axis M is
// P here). B08.nd2 is the one file trimming changes: T92, P2, Z5 here, T 81 once trimmed.
TEST(ND2CorpusOpen, LoopsMatchInventory)
{
    std::vector<std::string> skipped;
    int compared = 0;
    for (const Row& row : readTsv("nd2-loops.tsv")) {
        const std::string& file = row.at("file");
        if (absent(file, skipped)) {
            continue;
        }
        SCOPED_TRACE(file);
        const FileReader reader(corpusPath(file));
        const nd2::ChunkFile chunks = nd2::ChunkFile::read(reader);
        std::vector<std::string> names;
        if (chunks.versionMajor() < 3) {
            names.push_back("ImageMetadata!");
        }
        names.push_back("ImageMetadataLV!");
        nd2::Experiment experiment;
        for (const std::string& name : names) {
            if (const nd2::ChunkHeader* header = chunks.find(name)) {
                const nd2::Variant tree = nd2::decodeMetadata(chunks.readData(reader, *header));
                experiment = nd2::Experiment::parse(&tree);
                break;
            }
        }
        EXPECT_EQ(row.at("loops"), loopsText(experiment));
        if (file == "zenodo-16921650/B08.nd2") {
            EXPECT_EQ("T92 P2 Z5", loopsText(experiment));
        }
        ++compared;
    }
    reportSkipped(skipped);
    if (skipped.empty()) {
        EXPECT_EQ(55, compared);
    }
    ND2_SKIP_IF_ANY_ABSENT(skipped);
}

// Three 7x5 regions of up to six planes of every series, against the CRC-32 of the bytes
// Bio-Formats returned (confirmed there by SHA-256; for VPA002, where Bio-Formats is
// wrong, the bytes nd2spec and nd2 agree on). That includes the 30 regions Bio-Formats
// read from the surplus time points of old_cells_02, rfp_h2a_cells_02 and jonas/3, which
// the driver exposes (R29, design 4.5). A region in a frame the driver ignores would be
// counted, not read; there is none, since wherever the driver's T differs from
// Bio-Formats' it is the larger.
TEST(ND2CorpusRead, Regions)
{
    std::vector<std::string> skipped;
    std::map<std::string, int> beyondT;
    int matches = 0;
    int mismatches = 0;
    for (const auto& [file, rows] : byFile(readTsv("nd2-regions.tsv"))) {
        if (absent(file, skipped)) {
            continue;
        }
        mismatches += checkRegions(file, rows, matches, beyondT);
    }
    reportSkipped(skipped);
    int notRead = 0;
    for (const auto& [file, count] : beyondT) {
        notRead += count;
        std::printf("  %d regions beyond T in %s\n", count, file.c_str());
    }
    std::printf("%d regions matched, %d mismatched, %d in ignored frames\n", matches, mismatches, notRead);
    EXPECT_EQ(0, mismatches);
    EXPECT_TRUE(beyondT.empty());
    if (skipped.empty()) {
        EXPECT_EQ(3117, matches);
    }
    ND2_SKIP_IF_ANY_ABSENT(skipped);
}

// 117 of the 684 chunk-map entries are stale (FD §3.3); the frames are found by scan. The
// regions of the eight relocated planes are in nd2-regions.tsv.
TEST(ND2CorpusOpen, StaleMapRecovered)
{
    SLIDEIO_SKIP_IF_IMAGE_MISSING(TestTools::getTestImagePath("nd2", "aryeh/b16_pdtB+y50__crop.nd2"));
    const auto slide = openCorpus(kStaleMap);
    ASSERT_TRUE(slide);
    EXPECT_EQ(12, slide->getNumScenes());
    std::vector<Row> rows;
    for (const Row& row : readTsv("nd2-regions.tsv")) {
        if (row.at("file") == kStaleMap) {
            rows.push_back(row);
        }
    }
    ASSERT_EQ(240u, rows.size());
    int matches = 0;
    std::map<std::string, int> beyondT;
    EXPECT_EQ(0, checkRegions(kStaleMap, rows, matches, beyondT));
    EXPECT_TRUE(beyondT.empty());
    EXPECT_EQ(240, matches);
}

// The 23641 x 23641 x 2 uint16 slide (one 2.2 GB frame) read whole to 512 x 512 stays
// within the default 16 Mi-pixel source budget, and equals reading it at 2048 x 2048 and
// area-filtering that to 512 (the two area filters cover the same source intervals) to
// within the rounding of the second one.
TEST(ND2CorpusRead, LargeSlideThumbnail)
{
    const std::string path = TestTools::getTestImagePath(
        "nd2", "zenodo-16628927/Slide4-4_Region0001_Channel20xDAPI,20xCy3_Seq0027.nd2");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    ResampleLimits limits;
    limits.stats = std::make_shared<ResampleStats>();
    const auto slide = ND2Slide::openFile(path, limits);
    ASSERT_TRUE(slide);
    const auto scene = slide->getScene(0);
    const cv::Rect rect = scene->getRect();
    ASSERT_EQ(cv::Rect(0, 0, 23641, 23641), rect);
    cv::Mat thumbnail;
    scene->readResampledBlockChannels(rect, cv::Size(512, 512), {}, thumbnail);
    ASSERT_EQ(cv::Size(512, 512), thumbnail.size());
    ASSERT_EQ(CV_16UC2, thumbnail.type());
    EXPECT_GT(limits.stats->peakSourcePixels.load(), 0);
    EXPECT_LE(limits.stats->peakSourcePixels.load(), int64_t(16) << 20);

    cv::Mat large, reduced;
    scene->readResampledBlockChannels(rect, cv::Size(2048, 2048), {}, large);
    ASSERT_EQ(cv::Size(2048, 2048), large.size());
    cv::resize(large, reduced, cv::Size(512, 512), 0, 0, cv::INTER_AREA);
    EXPECT_LE(cv::norm(thumbnail, reduced, cv::NORM_INF), 1.);
    EXPECT_GT(cv::countNonZero(thumbnail.reshape(1)), 0);
}

TEST(ND2CorpusRead, ConcurrentIdentity)
{
    const std::string path = TestTools::getTestImagePath("nd2", "zenodo/bioformats/190821_L1_IRS-Akt_CS2003.nd2");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    ND2ImageDriver driver;
    TestTools::concurrentReadIdentityTestAllScenes(path, driver);
}

// Plane timestamps of two time-lapse files against nd2's frame times (nd2-times.tsv):
// 190821_L1_IRS-Akt_CS2003.nd2 (T31 x P46, Ver3.0; its 10 surplus frames are a partial
// time point and ignored) and B08.nd2 (NE time loop, T trimmed from 92 to 81). A plane
// whose frame is absent would have no reference row and be skipped; there is none. Offsets are non-negative and non-decreasing in T; differences between
// planes equal nd2's relative times to 1 us; acquisition + offset equals nd2's absolute
// Julian day to 1 ms (a Julian day near 2.46e6 in a double keeps about 40 us).
TEST(ND2CorpusOpen, Timestamps)
{
    const std::string files[] = {
        TestTools::getTestImagePath("nd2", "zenodo/bioformats/190821_L1_IRS-Akt_CS2003.nd2"),
        TestTools::getTestImagePath("nd2", "zenodo-16921650/B08.nd2"),
    };
    for (const std::string& path : files) {
        SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    }
    // file -> (series, t, z) -> (relative ms, absolute Julian day)
    std::map<std::string, std::map<std::tuple<int, int, int>, std::pair<double, double>>> reference;
    for (const Row& row : readTsv("nd2-times.tsv")) {
        reference[row.at("file")][{toInt(row.at("series")), toInt(row.at("t")), toInt(row.at("z"))}] =
            {std::stod(row.at("relativeMs")), std::stod(row.at("absoluteJdn"))};
    }
    ASSERT_EQ(2u, reference.size());
    int planesCompared = 0;
    double worstRelative = 0, worstAbsolute = 0;
    for (const auto& [file, planes] : reference) {
        SCOPED_TRACE(file);
        const auto slide = openCorpus(file);
        ASSERT_TRUE(slide);
        int planesPresent = 0;
        for (int s = 0; s < slide->getNumScenes(); ++s) {
            const auto scene = slide->getScene(s);
            SCOPED_TRACE(scene->getName());
            ASSERT_TRUE(scene->hasPlaneTimestamps());
            const int numT = scene->getNumTFrames();
            const int numZ = scene->getNumZSlices();
            for (int c = 0; c < scene->getNumChannels(); ++c) {
                for (int z = 0; z < numZ; ++z) {
                    double previous = 0.;
                    for (int t = 0; t < numT; ++t) {
                        if (planes.find({s, t, z}) == planes.end()) {
                            continue;   // absent frame
                        }
                        const double offset = scene->getPlaneTimestamp(t, c, z);
                        EXPECT_GE(offset, 0.);
                        EXPECT_GE(offset, previous) << "t " << t << " c " << c << " z " << z;
                        previous = offset;
                    }
                }
            }
            const auto first = planes.find({s, 0, 0});
            ASSERT_NE(planes.end(), first);
            const int64_t acquisition = scene->getAcquisitionTime();
            ASSERT_GT(acquisition, 0);
            for (int t = 0; t < numT; ++t) {
                for (int z = 0; z < numZ; ++z) {
                    const auto it = planes.find({s, t, z});
                    if (it == planes.end()) {
                        continue;   // absent frame
                    }
                    ++planesPresent;
                    const auto [relativeMs, julianDay] = it->second;
                    const double offset = scene->getPlaneTimestamp(t, 0, z);
                    const double relative = offset - scene->getPlaneTimestamp(0, 0, 0)
                        - (relativeMs - first->second.first) / 1000.;
                    // Unix seconds of nd2's absolute time, split so the large common part
                    // cancels exactly.
                    const double unixSeconds = (julianDay - 2440587.5) * 86400.;
                    const double whole = std::floor(unixSeconds);
                    const double absolute = static_cast<double>(acquisition - static_cast<int64_t>(whole))
                        + offset - (unixSeconds - whole);
                    worstRelative = std::max(worstRelative, std::abs(relative));
                    worstAbsolute = std::max(worstAbsolute, std::abs(absolute));
                    EXPECT_LE(std::abs(relative), 1e-6) << "t " << t << " z " << z;
                    EXPECT_LE(std::abs(absolute), 1e-3) << "t " << t << " z " << z;
                    for (int c = 1; c < scene->getNumChannels(); ++c) {
                        EXPECT_EQ(offset, scene->getPlaneTimestamp(t, c, z));
                    }
                    ++planesCompared;
                }
            }
        }
        // Every reference frame belongs to a plane of a scene.
        EXPECT_EQ(static_cast<int>(planes.size()), planesPresent);
    }
    std::printf("%d planes compared; worst relative %.3g s, worst absolute %.3g s\n",
                planesCompared, worstRelative, worstAbsolute);
    EXPECT_EQ(1426 + 810, planesCompared);
}
