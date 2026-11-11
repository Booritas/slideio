// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// The LIF driver against the seven corpus files under $SLIDEIO_IMAGES_PATH/lif (design
// §8.2). The references are committed in data/:
//   lif-scenes.tsv      Bio-Formats series        (auxfiles/lif-fixtures/LifScenes.java)
//   lif-regions.tsv     Bio-Formats 7x5 regions   (auxfiles/lif-fixtures/make_region_reference.py)
//   lif-timestamps.tsv  liffile plane timestamps  (auxfiles/lif-fixtures/make_timestamp_reference.py)
// Every suite here is named LIFCorpus*; CI excludes them.
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
#include "slideio/drivers/lif/lifimagedriver.hpp"
#include "slideio/drivers/lif/lifslide.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/cvslide.hpp"
#include "tests/testlib/testtools.hpp"

using namespace slideio;

namespace
{
    const char* const kFiles[] = {
        "bioformats/imagesc-110520/AMR1.lif",
        "bioformats/imagesc-30856/20191025 Test FRET 585. 423, 426.lif",
        "bioformats/michael/PR2729_frameOrderCombinedScanTypes.lif",
        "bioformats/seanwarren/150519_FRAP_test_ROIs_chromagreen/150519_FRAP_test_ROIs_chromagreen.lif",
        "bioformats/zenodo-6606445/Project007.lif",
        "figshare/IT4_27_02_15.lif",
        "zenodo/intro/Intro.lif",
    };
    const char* const kAmr1 = "bioformats/imagesc-110520/AMR1.lif";
    const char* const kFrap =
        "bioformats/seanwarren/150519_FRAP_test_ROIs_chromagreen/150519_FRAP_test_ROIs_chromagreen.lif";
    const char* const kProject007 = "bioformats/zenodo-6606445/Project007.lif";
    const char* const kIntro = "zenodo/intro/Intro.lif";

    std::string corpusPath(const std::string& rel)
    {
        return TestTools::getTestImagePath("lif", rel);
    }

    using Row = std::map<std::string, std::string>;

    // A tab-separated reference with a header row.
    std::vector<Row> readTsv(const std::string& name)
    {
        const std::string path = std::string(LIF_TEST_DATA_DIR) + "/" + name;
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

    int toInt(const std::string& s) { return std::stoi(s); }

    std::shared_ptr<LIFSlide> openCorpus(const std::string& rel)
    {
        LIFImageDriver driver;
        return std::dynamic_pointer_cast<LIFSlide>(driver.openFile(corpusPath(rel)));
    }

    DataType bioFormatsType(const std::string& name)
    {
        static const std::map<std::string, DataType> types = {
            {"uint8", DataType::DT_Byte}, {"int8", DataType::DT_Int8},
            {"uint16", DataType::DT_UInt16}, {"int16", DataType::DT_Int16},
            {"uint32", DataType::DT_UInt32}, {"int32", DataType::DT_Int32},
            {"float", DataType::DT_Float32}, {"double", DataType::DT_Float64},
        };
        const auto it = types.find(name);
        EXPECT_NE(types.end(), it) << "unknown Bio-Formats pixel type " << name;
        return it == types.end() ? DataType::DT_Unknown : it->second;
    }

    // Bio-Formats size in micrometres, or 0 for an empty field (none reported).
    double micrometres(const std::string& s)
    {
        return s.empty() ? 0. : std::stod(s);
    }

    bool sameResolution(double driverUm, double bioFormatsUm)
    {
        if (driverUm == 0. || bioFormatsUm == 0.) {
            return driverUm == bioFormatsUm;
        }
        return std::abs(driverUm - bioFormatsUm) <= 1e-9 * std::abs(bioFormatsUm);
    }
}

// Scene list, names, geometry, type and calibration against Bio-Formats' series. Scenes
// map to series by index: the driver expands tile fields in document order as
// Bio-Formats does (design §4.5), and every image in the corpus is supported, so nothing
// shifts the indices -- a skipped image fails the test rather than being remapped around.
//
// Resolution agrees to 1e-9 relative everywhere but one place, recorded in design §6.2:
// the Z step of the Z stacks of IT4_27_02_15.lif. The driver divides each image's Z
// Length by N-1; Bio-Formats reports the same step for every image of that file, the
// scanner's recorded one, and the two differ in the seventh significant digit. Those
// pairs are printed and counted; any other disagreement fails.
TEST(LIFCorpusOpen, ScenesMatchBioFormats)
{
    for (const char* file : kFiles) {
        SLIDEIO_SKIP_IF_IMAGE_MISSING(corpusPath(file));
    }
    std::map<std::string, std::vector<Row>> byFile;
    for (const Row& row : readTsv("lif-scenes.tsv")) {
        byFile[row.at("file")].push_back(row);
    }
    ASSERT_EQ(std::size(kFiles), byFile.size());
    size_t seriesChecked = 0;
    std::set<int> recordedZDisagreements;
    for (const auto& [file, rows] : byFile) {
        SCOPED_TRACE(file);
        const auto slide = openCorpus(file);
        ASSERT_TRUE(slide);
        for (const std::string& skipped : slide->skippedImages()) {
            ADD_FAILURE() << "skipped image: " << skipped;
        }
        ASSERT_EQ(static_cast<int>(rows.size()), slide->getNumScenes());
        for (size_t i = 0; i < rows.size(); ++i) {
            const Row& row = rows[i];
            const int series = toInt(row.at("series"));
            ASSERT_EQ(static_cast<int>(i), series);
            SCOPED_TRACE("series " + row.at("series") + " " + row.at("name"));
            const auto scene = slide->getScene(series);
            ASSERT_TRUE(scene);

            // Bio-Formats names every field of a tile scan after the image; the driver
            // appends "/Field <k>" (design §4.5). A run of equal names is one image.
            const std::string& bfName = row.at("name");
            size_t first = i, last = i;
            while (first > 0 && rows[first - 1].at("name") == bfName) --first;
            while (last + 1 < rows.size() && rows[last + 1].at("name") == bfName) ++last;
            const std::string expectedName = first == last
                ? bfName : bfName + "/Field " + std::to_string(i - first + 1);
            EXPECT_EQ(expectedName, scene->getName());

            const cv::Rect rect = scene->getRect();
            EXPECT_EQ(toInt(row.at("width")), rect.width);
            EXPECT_EQ(toInt(row.at("height")), rect.height);
            EXPECT_EQ(toInt(row.at("c")), scene->getNumChannels());
            EXPECT_EQ(toInt(row.at("z")), scene->getNumZSlices());
            EXPECT_EQ(toInt(row.at("t")), scene->getNumTFrames());
            const DataType type = bioFormatsType(row.at("pixelType"));
            for (int c = 0; c < scene->getNumChannels(); ++c) {
                EXPECT_EQ(type, scene->getChannelDataType(c)) << "channel " << c;
                EXPECT_EQ("", scene->getChannelName(c)) << "channel " << c;
            }

            const Resolution res = scene->getResolution();
            const double driverUm[3] = {res.x * 1e6, res.y * 1e6, scene->getZSliceResolution() * 1e6};
            const char* const columns[3] = {"physX_um", "physY_um", "physZ_um"};
            for (int axis = 0; axis < 3; ++axis) {
                const double reference = micrometres(row.at(columns[axis]));
                if (sameResolution(driverUm[axis], reference)) {
                    continue;
                }
                std::printf("resolution %s %s series %d (%s): driver %.17g um, Bio-Formats %.17g um\n",
                            columns[axis], file.c_str(), series, scene->getName().c_str(),
                            driverUm[axis], reference);
                const bool recorded = file == "figshare/IT4_27_02_15.lif" && axis == 2
                    && std::abs(driverUm[axis] - reference) <= 1e-6 * reference;
                if (recorded) {
                    recordedZDisagreements.insert(series);
                }
                else {
                    ADD_FAILURE() << columns[axis] << ": driver " << driverUm[axis]
                                  << " um, Bio-Formats " << reference << " um";
                }
            }
            ++seriesChecked;
        }
    }
    EXPECT_EQ(137u, seriesChecked);
    // Series 0-20 of IT4 are its Z stacks; 21-25 are single planes (no Z step).
    std::set<int> zStacks;
    for (int series = 0; series <= 20; ++series) {
        zStacks.insert(series);
    }
    EXPECT_EQ(zStacks, recordedZDisagreements);
}

// Three 7x5 regions of every plane of every series, against the CRC-32 of the bytes
// Bio-Formats returned (confirmed there by SHA-256).
TEST(LIFCorpusRead, RegionsMatchBioFormats)
{
    for (const char* file : kFiles) {
        SLIDEIO_SKIP_IF_IMAGE_MISSING(corpusPath(file));
    }
    const std::vector<Row> rows = readTsv("lif-regions.tsv");
    ASSERT_EQ(9264u, rows.size());
    std::string openFile;
    std::shared_ptr<LIFSlide> slide;
    int matches = 0;
    int mismatches = 0;
    for (const Row& row : rows) {
        if (row.at("file") != openFile) {
            openFile = row.at("file");
            slide = openCorpus(openFile);
            ASSERT_TRUE(slide) << openFile;
        }
        const int series = toInt(row.at("series"));
        const int z = toInt(row.at("z"));
        const int c = toInt(row.at("c"));
        const int t = toInt(row.at("t"));
        const cv::Rect rect(toInt(row.at("rx")), toInt(row.at("ry")), toInt(row.at("rw")), toInt(row.at("rh")));
        const auto scene = slide->getScene(series);
        cv::Mat raster;
        scene->read4DBlockChannels(rect, {c}, cv::Range(z, z + 1), cv::Range(t, t + 1), raster);
        ASSERT_EQ(rect.width, raster.cols);
        ASSERT_EQ(rect.height, raster.rows);
        ASSERT_EQ(1, raster.channels());
        ASSERT_TRUE(raster.isContinuous());
        const uLong crc = crc32(0L, raster.data, static_cast<uInt>(raster.total() * raster.elemSize()));
        char hex[9];
        std::snprintf(hex, sizeof(hex), "%08lx", static_cast<unsigned long>(crc));
        if (row.at("crc32") == hex) {
            ++matches;
        }
        else if (++mismatches <= 10) {
            ADD_FAILURE() << openFile << " series " << series << " z " << z << " c " << c << " t " << t
                          << " at (" << rect.x << "," << rect.y << "): crc " << hex
                          << ", Bio-Formats " << row.at("crc32");
        }
    }
    std::printf("%d regions matched, %d mismatched\n", matches, mismatches);
    EXPECT_EQ(9264, matches);
    EXPECT_EQ(0, mismatches);
}

// AMR1 declares Resolution="12" on its uint16 channels.
TEST(LIFCorpusOpen, SignificantBitsAmr1)
{
    SLIDEIO_SKIP_IF_IMAGE_MISSING(corpusPath(kAmr1));
    const auto slide = openCorpus(kAmr1);
    ASSERT_TRUE(slide);
    ASSERT_EQ(27, slide->getNumScenes());
    for (int s = 0; s < slide->getNumScenes(); ++s) {
        const auto scene = slide->getScene(s);
        for (int c = 0; c < scene->getNumChannels(); ++c) {
            EXPECT_EQ(12, scene->getChannelSignificantBits(c)) << scene->getName() << " channel " << c;
        }
    }
}

// Plane timestamps on the FRAP time series and on Intro (whose tile scans give one
// timestamp per field), against liffile. liffile floors each FILETIME to a millisecond,
// so the driver's exact time acquisition + offset must lie in [liffile, liffile + 1 ms):
// that bounds every plane, and with it every difference between planes, by the
// precision liffile keeps. Images with a list that is not one entry per plane are not
// in the reference; the driver must report no timestamps for them.
TEST(LIFCorpusOpen, Timestamps)
{
    SLIDEIO_SKIP_IF_IMAGE_MISSING(corpusPath(kFrap));
    SLIDEIO_SKIP_IF_IMAGE_MISSING(corpusPath(kIntro));

    // file -> scene name -> (t, c, z) -> liffile seconds (as written, ms precision)
    using Planes = std::map<std::tuple<int, int, int>, std::string>;
    std::map<std::string, std::map<std::string, Planes>> reference;
    {
        std::map<std::pair<std::string, std::string>, std::set<int>> fields;
        const std::vector<Row> rows = readTsv("lif-timestamps.tsv");
        ASSERT_EQ(461u, rows.size());
        for (const Row& row : rows) {
            fields[{row.at("file"), row.at("image")}].insert(toInt(row.at("m")));
        }
        for (const Row& row : rows) {
            const auto& m = fields[{row.at("file"), row.at("image")}];
            std::string scene = row.at("image");
            if (m.size() > 1) {
                scene += "/Field " + std::to_string(toInt(row.at("m")) + 1);
            }
            reference[row.at("file")][scene][{toInt(row.at("t")), toInt(row.at("c")), toInt(row.at("z"))}] =
                row.at("seconds");
        }
    }

    int planesCompared = 0;
    for (const char* file : {kFrap, kIntro}) {
        SCOPED_TRACE(file);
        const auto slide = openCorpus(file);
        ASSERT_TRUE(slide);
        const auto& scenes = reference[file];
        int scenesWithTimestamps = 0;
        for (int s = 0; s < slide->getNumScenes(); ++s) {
            const auto scene = slide->getScene(s);
            SCOPED_TRACE(scene->getName());
            const auto it = scenes.find(scene->getName());
            if (it == scenes.end()) {
                EXPECT_FALSE(scene->hasPlaneTimestamps());
                continue;
            }
            ASSERT_TRUE(scene->hasPlaneTimestamps());
            ++scenesWithTimestamps;
            const int numT = scene->getNumTFrames();
            const int numC = scene->getNumChannels();
            const int numZ = scene->getNumZSlices();
            ASSERT_EQ(static_cast<size_t>(numT * numC * numZ), it->second.size());

            // Non-negative, and non-decreasing in T.
            for (int c = 0; c < numC; ++c) {
                for (int z = 0; z < numZ; ++z) {
                    double previous = 0.;
                    for (int t = 0; t < numT; ++t) {
                        const double offset = scene->getPlaneTimestamp(t, c, z);
                        EXPECT_GE(offset, 0.);
                        EXPECT_GE(offset, previous) << "t " << t << " c " << c << " z " << z;
                        previous = offset;
                    }
                }
            }

            const int64_t acquisition = scene->getAcquisitionTime();
            double earliest = 1e300;
            for (const auto& [plane, seconds] : it->second) {
                const auto [t, c, z] = plane;
                const double expected = std::stod(seconds);
                earliest = std::min(earliest, expected);
                // Exact time relative to the reference, which is far from double's
                // precision limit only once the large common part is removed.
                const int64_t whole = std::stoll(seconds.substr(0, seconds.find('.')));
                const double fraction = std::stod(seconds.substr(seconds.find('.')));
                const double delta = static_cast<double>(acquisition - whole)
                    + scene->getPlaneTimestamp(t, c, z) - fraction;
                EXPECT_GE(delta, -1e-6) << "t " << t << " c " << c << " z " << z << " liffile " << seconds;
                EXPECT_LT(delta, 1e-3 + 1e-6) << "t " << t << " c " << c << " z " << z << " liffile " << seconds;
                ++planesCompared;
            }
            EXPECT_NEAR(std::floor(earliest), static_cast<double>(acquisition), 1.);
        }
        EXPECT_EQ(static_cast<int>(scenes.size()), scenesWithTimestamps);
    }
    EXPECT_EQ(461, planesCompared);
}

// Magnification, printed per file and checked against the values in the files. Six
// files carry it on the ATLConfocalSettingDefinition directly under each image's
// "HardwareSetting" attachment; FRET-Efficiency, a computed image, has no attachment.
// IT4_27_02_15.lif has a "HardwareSettingList" attachment with the definition only
// inside LDM_Block_Sequential: 100 on its 21 Z stacks, 5 on the five *_5x images.
TEST(LIFCorpusOpen, Magnification)
{
    for (const char* file : kFiles) {
        SLIDEIO_SKIP_IF_IMAGE_MISSING(corpusPath(file));
    }
    // file -> magnification -> scene count
    const std::map<std::string, std::map<double, int>> expected = {
        {"bioformats/imagesc-110520/AMR1.lif", {{26.25, 27}}},
        {"bioformats/imagesc-30856/20191025 Test FRET 585. 423, 426.lif", {{0., 1}, {63., 7}}},
        {"bioformats/michael/PR2729_frameOrderCombinedScanTypes.lif", {{10., 4}}},
        {kFrap, {{63., 8}}},
        {kProject007, {{63., 7}}},
        {"figshare/IT4_27_02_15.lif", {{5., 5}, {100., 21}}},
        {kIntro, {{10., 51}, {25., 6}}},
    };
    for (const char* file : kFiles) {
        SCOPED_TRACE(file);
        const auto slide = openCorpus(file);
        ASSERT_TRUE(slide);
        std::map<double, int> values;
        for (int s = 0; s < slide->getNumScenes(); ++s) {
            const auto scene = slide->getScene(s);
            ++values[scene->getMagnification()];
            if (scene->getMagnification() == 0.) {
                std::cout << "  no magnification: " << scene->getName() << std::endl;
            }
        }
        std::cout << file << ":";
        for (const auto& [value, count] : values) {
            std::cout << " " << value << " (" << count << " scenes)";
        }
        std::cout << std::endl;
        EXPECT_EQ(expected.at(file), values);
    }
}

TEST(LIFCorpusRead, ConcurrentIdentity)
{
    SLIDEIO_SKIP_IF_IMAGE_MISSING(corpusPath(kProject007));
    LIFImageDriver driver;
    TestTools::concurrentReadIdentityTestAllScenes(corpusPath(kProject007), driver);
}

// An edge read reaching past the image is background (0) there and the image inside;
// a level read at level 0 equals the plain read (one level, but the contract holds).
TEST(LIFCorpusRead, EdgeAndLevelReads)
{
    SLIDEIO_SKIP_IF_IMAGE_MISSING(corpusPath(kProject007));
    const auto slide = openCorpus(kProject007);
    ASSERT_TRUE(slide);
    const auto scene = slide->getScene(0);
    const cv::Rect rect = scene->getRect();
    const cv::Rect edge(rect.width - 16, rect.height - 16, 32, 32);
    cv::Mat edgeRaster;
    scene->readBlockChannels(edge, {1}, edgeRaster);
    ASSERT_EQ(32, edgeRaster.cols);
    ASSERT_EQ(32, edgeRaster.rows);
    cv::Mat inside;
    scene->readBlockChannels(cv::Rect(rect.width - 16, rect.height - 16, 16, 16), {1}, inside);
    ASSERT_GT(cv::countNonZero(inside), 0) << "the corner must not be blank, or the check below proves nothing";
    EXPECT_EQ(0, cv::norm(edgeRaster(cv::Rect(0, 0, 16, 16)), inside, cv::NORM_INF));
    EXPECT_EQ(0, cv::countNonZero(edgeRaster(cv::Rect(16, 0, 16, 32))));
    EXPECT_EQ(0, cv::countNonZero(edgeRaster(cv::Rect(0, 16, 16, 16))));

    const cv::Rect block(100, 120, 200, 150);
    const cv::Size size(100, 75);
    cv::Mat scaled, level;
    scene->readResampledBlockChannels(block, size, {}, scaled);
    scene->readResampledLevelBlockChannels(0, block, size, {}, level);
    ASSERT_EQ(scaled.size(), level.size());
    ASSERT_EQ(scaled.type(), level.type());
    EXPECT_EQ(0, cv::norm(scaled, level, cv::NORM_INF));
}
