// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// Pyramidal tiffs through the GDAL driver. Every fixture is written at run time
// with raw libtiff calls (gdaltiffwriter.cpp), and every level written is the
// exact raster the test compares against, so a level read must match bit for bit.
#include "slideio/drivers/gdal/gdalimagedriver.hpp"
#include "slideio/drivers/gdal/gdaltiffscene.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/core/levelinfo.hpp"
#include "slideio/core/tools/contextpool.hpp"
#include "slideio/core/tools/tempfile.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/imagetools/libtiff.hpp"
#include "slideio/slideio/slideio.hpp"
#include "tests/main/gdaltiffwriter.hpp"

#include <gtest/gtest.h>
#include "slideio/imagetools/imagetools.hpp"
#include <opencv2/imgproc.hpp>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <random>
#include <set>
#include <system_error>
#include <thread>

using namespace gdaltest;

namespace
{
    // The reduced levels of base, each an INTER_AREA resize of the base itself.
    std::vector<cv::Mat> makePyramid(const cv::Mat& base, const std::vector<cv::Size>& reduced)
    {
        std::vector<cv::Mat> levels{base};
        for (const cv::Size& size : reduced) {
            cv::Mat level;
            cv::resize(base, level, size, 0, 0, cv::INTER_AREA);
            levels.push_back(level);
        }
        return levels;
    }

    // Writes a file through a callback that receives the open handle. mode is
    // TIFFOpen's: "w" for a classic tiff, "w8" for a BigTIFF. The handle is closed
    // even when body fails, so a caller checks HasFatalFailure() afterwards.
    void writeTiff(const std::string& path, const std::function<void(libtiff::TIFF*)>& body,
                   const char* mode = "w")
    {
        libtiff::TIFF* tiff = libtiff::TIFFOpen(path.c_str(), mode);
        ASSERT_TRUE(tiff != nullptr);
        body(tiff);
        libtiff::TIFFClose(tiff);
    }

    // The base as a tiled directory carrying every other level as a tiled SubIFD.
    void writeSubIfdPyramid(const std::string& path, const std::vector<cv::Mat>& levels, int tileSize,
                            bool planar = false, uint8_t padValue = 255, const char* mode = "w")
    {
        writeTiff(path, [&](libtiff::TIFF* tiff) {
            const int numSubIfds = static_cast<int>(levels.size()) - 1;
            ASSERT_NO_FATAL_FAILURE(writeTiledDirectory(tiff, levels[0], tileSize, planar, padValue, 0, numSubIfds));
            for (int level = 1; level < static_cast<int>(levels.size()); ++level) {
                ASSERT_NO_FATAL_FAILURE(writeTiledDirectory(tiff, levels[level], tileSize, planar, padValue, 1));
            }
        }, mode);
    }

    std::shared_ptr<slideio::CVSlide> openGdal(const std::string& path)
    {
        slideio::GDALImageDriver driver;
        return driver.openFile(path);
    }

    // Reads the full extent of every level and compares it with what was written.
    void expectLevelsRead(const std::shared_ptr<slideio::CVScene>& scene, const std::vector<cv::Mat>& levels)
    {
        ASSERT_EQ(static_cast<int>(levels.size()), scene->getNumZoomLevels());
        for (int level = 0; level < static_cast<int>(levels.size()); ++level) {
            SCOPED_TRACE(::testing::Message() << "level " << level);
            const cv::Mat& expected = levels[level];
            cv::Mat raster;
            scene->readResampledLevelBlockChannels(level, {0, 0, expected.cols, expected.rows},
                                                   expected.size(), {}, raster);
            ASSERT_EQ(expected.size(), raster.size());
            ASSERT_EQ(expected.type(), raster.type());
            EXPECT_EQ(0, cv::norm(expected, raster, cv::NORM_INF));
        }
    }

    void expectLevelInfo(const std::shared_ptr<slideio::CVScene>& scene, const std::vector<cv::Mat>& levels,
                         const std::vector<cv::Size>& tileSizes)
    {
        ASSERT_EQ(static_cast<int>(levels.size()), scene->getNumZoomLevels());
        for (int level = 0; level < static_cast<int>(levels.size()); ++level) {
            SCOPED_TRACE(::testing::Message() << "level " << level);
            const slideio::LevelInfo* info = scene->getZoomLevelInfo(level);
            ASSERT_TRUE(info != nullptr);
            EXPECT_EQ(level, info->getLevel());
            EXPECT_EQ(levels[level].cols, info->getSize().width);
            EXPECT_EQ(levels[level].rows, info->getSize().height);
            EXPECT_EQ(tileSizes[level].width, info->getTileSize().width);
            EXPECT_EQ(tileSizes[level].height, info->getTileSize().height);
            EXPECT_DOUBLE_EQ(static_cast<double>(levels[level].cols) / levels[0].cols, info->getScale());
        }
    }

    const std::vector<cv::Size> kSubIfdLevels{{500, 400}, {250, 200}};

    struct ReadSpec
    {
        int level;
        cv::Rect rect;
        cv::Size size;
    };

    enum class ReadPath
    {
        // readResampledLevelBlockChannels on a random level.
        Level,
        // readResampledBlock on the base, leaving level selection to the scene
        // (readResampledBlockChannelsEx).
        ScaledBlock
    };

    void readSpec(const std::shared_ptr<slideio::CVScene>& scene, ReadPath path, const ReadSpec& spec,
                  cv::Mat& raster)
    {
        if (path == ReadPath::Level) {
            scene->readResampledLevelBlockChannels(spec.level, spec.rect, spec.size, {}, raster);
        } else {
            scene->readResampledBlock(spec.rect, spec.size, raster);
        }
    }

    // 8 threads x 50 reads, each a random level, rect and output scale from a fixed
    // per-thread seed. Every result must equal the same read made serially first.
    void expectConcurrentReadsMatchSerial(const std::shared_ptr<slideio::CVScene>& scene,
                                          ReadPath path = ReadPath::Level)
    {
        constexpr int kThreads = 8;
        constexpr int kReads = 50;
        const int numLevels = path == ReadPath::Level ? scene->getNumZoomLevels() : 1;
        std::vector<std::vector<ReadSpec>> specs(kThreads);
        std::vector<std::vector<cv::Mat>> expected(kThreads);
        for (int t = 0; t < kThreads; ++t) {
            std::mt19937 rng(1000 + t);
            auto pick = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };
            for (int i = 0; i < kReads; ++i) {
                const int level = pick(0, numLevels - 1);
                const cv::Size levelSize = scene->getZoomLevelInfo(level)->getSize();
                const int w = pick(1, levelSize.width);
                const int h = pick(1, levelSize.height);
                const cv::Rect rect(pick(0, levelSize.width - w), pick(0, levelSize.height - h), w, h);
                const double scale = pick(25, 100) / 100.0;
                const cv::Size size(std::max(1, static_cast<int>(w * scale)),
                                    std::max(1, static_cast<int>(h * scale)));
                specs[t].push_back({level, rect, size});
                cv::Mat raster;
                readSpec(scene, path, specs[t].back(), raster);
                expected[t].push_back(raster);
            }
        }
        std::atomic<int> failures{0};
        auto reader = [&](int t) {
            try {
                for (int i = 0; i < kReads; ++i) {
                    const ReadSpec& spec = specs[t][i];
                    cv::Mat raster;
                    readSpec(scene, path, spec, raster);
                    if (raster.size() != expected[t][i].size() || raster.type() != expected[t][i].type()
                        || cv::norm(raster, expected[t][i], cv::NORM_INF) != 0) {
                        ++failures;
                    }
                }
            }
            catch (...) {
                ++failures;
            }
        };
        std::vector<std::thread> threads;
        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back(reader, t);
        }
        for (auto& thread : threads) {
            thread.join();
        }
        EXPECT_EQ(0, failures.load());
    }
}

// The SubIFD layout libvips and bfconvert write: one scene, three levels.
TEST(GDALTiffPyramid, subIfdPyramidOpensAsOneScene)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    const auto levels = makePyramid(makeRgbImage(1000, 800), kSubIfdLevels);
    ASSERT_NO_FATAL_FAILURE(writeSubIfdPyramid(path, levels, 256));

    auto slide = openGdal(path);
    ASSERT_EQ(1, slide->getNumScenes());
    auto scene = slide->getScene(0);
    EXPECT_EQ(cv::Rect(0, 0, 1000, 800), scene->getRect());
    EXPECT_EQ(3, scene->getNumChannels());
    expectLevelInfo(scene, levels, {{256, 256}, {256, 256}, {256, 256}});
    expectLevelsRead(scene, levels);
}

// The same SubIFD pyramid written as a BigTIFF: 64-bit offsets, including the
// SubIFD offsets themselves.
TEST(GDALTiffPyramid, bigTiffSubIfdPyramidOpensAsOneScene)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    const auto levels = makePyramid(makeRgbImage(1000, 800), kSubIfdLevels);
    ASSERT_NO_FATAL_FAILURE(writeSubIfdPyramid(path, levels, 256, false, 255, "w8"));
    {
        // The header says BigTIFF: version 43 rather than 42.
        std::ifstream file(path, std::ios::binary);
        unsigned char header[4] = {};
        file.read(reinterpret_cast<char*>(header), sizeof(header));
        ASSERT_TRUE(file.good());
        const int version = header[0] == 'I' ? header[2] | (header[3] << 8) : (header[2] << 8) | header[3];
        ASSERT_EQ(43, version);
    }

    auto slide = openGdal(path);
    ASSERT_EQ(1, slide->getNumScenes());
    auto scene = slide->getScene(0);
    EXPECT_EQ(cv::Rect(0, 0, 1000, 800), scene->getRect());
    expectLevelInfo(scene, levels, {{256, 256}, {256, 256}, {256, 256}});
    expectLevelsRead(scene, levels);
}

// The COG layout: reduced pages as top-level IFDs, and a mask page that is
// neither a scene nor a level.
TEST(GDALTiffPyramid, cogLayoutOpensAsOneScene)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    const auto levels = makePyramid(makeRgbImage(1000, 800), kSubIfdLevels);
    writeTiff(path, [&](libtiff::TIFF* tiff) {
        ASSERT_NO_FATAL_FAILURE(writeTiledDirectory(tiff, levels[0], 256, false, 255, 0));
        cv::Mat mask(800, 1000, CV_8UC1, cv::Scalar(255));
        ASSERT_NO_FATAL_FAILURE(writeTiledDirectory(tiff, mask, 256, false, 0, 4));
        ASSERT_NO_FATAL_FAILURE(writeTiledDirectory(tiff, levels[1], 256, false, 255, 1));
        ASSERT_NO_FATAL_FAILURE(writeTiledDirectory(tiff, levels[2], 256, false, 255, 1));
    });
    ASSERT_FALSE(::testing::Test::HasFatalFailure());

    auto slide = openGdal(path);
    ASSERT_EQ(1, slide->getNumScenes());
    auto scene = slide->getScene(0);
    expectLevelInfo(scene, levels, {{256, 256}, {256, 256}, {256, 256}});
    expectLevelsRead(scene, levels);
}

TEST(GDALTiffPyramid, uint16GrayCog)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    const auto levels = makePyramid(makeGrayImage16(600, 400), {{300, 200}});
    writeTiff(path, [&](libtiff::TIFF* tiff) {
        ASSERT_NO_FATAL_FAILURE(writeTiledDirectory(tiff, levels[0], 128, false, 255, 0));
        ASSERT_NO_FATAL_FAILURE(writeTiledDirectory(tiff, levels[1], 128, false, 255, 1));
    });
    ASSERT_FALSE(::testing::Test::HasFatalFailure());

    auto slide = openGdal(path);
    ASSERT_EQ(1, slide->getNumScenes());
    auto scene = slide->getScene(0);
    EXPECT_EQ(1, scene->getNumChannels());
    EXPECT_EQ(slideio::DataType::DT_UInt16, scene->getChannelDataType(0));
    expectLevelInfo(scene, levels, {{128, 128}, {128, 128}});
    expectLevelsRead(scene, levels);
}

// A planar level reads one plane per requested channel, in the order asked.
TEST(GDALTiffPyramid, planarPyramidChannelSubset)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    const auto levels = makePyramid(makeRgbImage(1000, 800), kSubIfdLevels);
    ASSERT_NO_FATAL_FAILURE(writeSubIfdPyramid(path, levels, 256, true));

    auto scene = openGdal(path)->getScene(0);
    ASSERT_EQ(3, scene->getNumZoomLevels());
    const cv::Mat& level1 = levels[1];
    cv::Mat raster;
    scene->readResampledLevelBlockChannels(1, {0, 0, level1.cols, level1.rows}, level1.size(), {2, 0}, raster);
    std::vector<cv::Mat> planes;
    cv::split(level1, planes);
    cv::Mat expected;
    cv::merge(std::vector<cv::Mat>{planes[2], planes[0]}, expected);
    ASSERT_EQ(expected.size(), raster.size());
    ASSERT_EQ(CV_8UC2, raster.type());
    EXPECT_EQ(0, cv::norm(expected, raster, cv::NORM_INF));
    // The whole level, every channel, planar.
    expectLevelsRead(scene, levels);
}

// Levels whose sizes are odd, so their right and bottom tiles are partial and the
// level is not an exact power-of-two reduction. A scaled read at the edge must be
// served by the smallest level, and the encoder's padding (255) must stay out.
TEST(GDALTiffPyramid, oddSizedLevelsScaledReadAtEdge)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    // Smooth gradients that are nowhere near 255 at the right and bottom edges, so
    // a resize may differ from another by rounding only, and padding interpolated
    // into the edge shows as a large difference.
    cv::Mat base(701, 1001, CV_8UC3);
    for (int y = 0; y < base.rows; ++y) {
        for (int x = 0; x < base.cols; ++x) {
            base.at<cv::Vec3b>(y, x) = cv::Vec3b(static_cast<uint8_t>(230 - x / 5),
                                                 static_cast<uint8_t>(20 + y / 5),
                                                 static_cast<uint8_t>(30 + (x + y) / 10));
        }
    }
    const auto levels = makePyramid(base, {{501, 351}, {251, 176}});
    ASSERT_NO_FATAL_FAILURE(writeSubIfdPyramid(path, levels, 64, false, 255));

    auto scene = openGdal(path)->getScene(0);
    ASSERT_EQ(3, scene->getNumZoomLevels());
    const cv::Rect rect(751, 501, 250, 200);
    const cv::Size size(63, 50);
    cv::Mat block;
    scene->readResampledBlock(rect, size, block);
    ASSERT_EQ(size, block.size());
    ASSERT_EQ(CV_8UC3, block.type());

    const cv::Mat& level2 = levels[2];
    cv::Rect levelRect;
    slideio::Tools::scaleRect(rect, static_cast<double>(level2.cols) / base.cols,
                              static_cast<double>(level2.rows) / base.rows, levelRect);
    levelRect &= cv::Rect(0, 0, level2.cols, level2.rows);
    cv::Mat expected;
    cv::resize(level2(levelRect), expected, size, 0, 0, cv::INTER_AREA);
    // Padding interpolated into the edge would differ from the expected block by
    // far more than rounding.
    EXPECT_LE(cv::norm(expected, block, cv::NORM_INF), 1.);
}

// A downscaled read of the whole scene into the size of level 2 is level 2 exactly,
// which only reading level 2 itself can produce.
TEST(GDALTiffPyramid, scaledReadPicksReducedLevel)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    const auto levels = makePyramid(makeRgbImage(1000, 800), kSubIfdLevels);
    ASSERT_NO_FATAL_FAILURE(writeSubIfdPyramid(path, levels, 256));

    auto scene = openGdal(path)->getScene(0);
    cv::Mat block;
    scene->readResampledBlock({0, 0, 1000, 800}, {250, 200}, block);
    ASSERT_EQ(levels[2].size(), block.size());
    ASSERT_EQ(levels[2].type(), block.type());
    EXPECT_EQ(0, cv::norm(levels[2], block, cv::NORM_INF));
}

// A striped base reports itself as one tile; its tiled reduced level reports its tiles.
TEST(GDALTiffPyramid, stripedBaseTiledLevels)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    const auto levels = makePyramid(makeRgbImage(600, 400), {{300, 200}});
    writeTiff(path, [&](libtiff::TIFF* tiff) {
        ASSERT_NO_FATAL_FAILURE(writeStripedDirectory(tiff, levels[0], 0, 1));
        ASSERT_NO_FATAL_FAILURE(writeTiledDirectory(tiff, levels[1], 64, false, 255, 1));
    });
    ASSERT_FALSE(::testing::Test::HasFatalFailure());

    auto slide = openGdal(path);
    ASSERT_EQ(1, slide->getNumScenes());
    auto scene = slide->getScene(0);
    expectLevelInfo(scene, levels, {{600, 400}, {64, 64}});
    expectLevelsRead(scene, levels);
    // A block of the striped base, with a channel subset.
    cv::Mat block;
    scene->readBlockChannels({100, 50, 200, 120}, {1}, block);
    cv::Mat expected;
    cv::extractChannel(levels[0]({100, 50, 200, 120}), expected, 1);
    ASSERT_EQ(expected.size(), block.size());
    EXPECT_EQ(0, cv::norm(expected, block, cv::NORM_INF));
}

// Two unrelated full-resolution pages stay two scenes of one level each.
TEST(GDALTiffPyramid, plainMultiPageUnchanged)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    const cv::Mat page0 = makeRgbImage(600, 500, 0);
    const cv::Mat page1 = makeRgbImage(600, 500, 100);
    writeTiff(path, [&](libtiff::TIFF* tiff) {
        ASSERT_NO_FATAL_FAILURE(writeTiledDirectory(tiff, page0, 64, false, 255));
        ASSERT_NO_FATAL_FAILURE(writeTiledDirectory(tiff, page1, 64, false, 255));
    });
    ASSERT_FALSE(::testing::Test::HasFatalFailure());

    auto slide = openGdal(path);
    ASSERT_EQ(2, slide->getNumScenes());
    expectLevelsRead(slide->getScene(0), {page0});
    expectLevelsRead(slide->getScene(1), {page1});
}

// Two COG-style pyramids one after the other: A, rA1, rA2, B, rB1, all top-level,
// the reduced ones flagged. Two scenes in file order, and the second reads from
// directories past the first.
TEST(GDALTiffPyramid, twoPyramidsInOneFileOpenAsTwoScenes)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    const auto levelsA = makePyramid(makeRgbImage(1000, 800, 0), {{500, 400}, {250, 200}});
    const auto levelsB = makePyramid(makeRgbImage(600, 600, 100), {{300, 300}});
    writeTiff(path, [&](libtiff::TIFF* tiff) {
        ASSERT_NO_FATAL_FAILURE(writeTiledDirectory(tiff, levelsA[0], 128, false, 255, 0));
        ASSERT_NO_FATAL_FAILURE(writeTiledDirectory(tiff, levelsA[1], 128, false, 255, 1));
        ASSERT_NO_FATAL_FAILURE(writeTiledDirectory(tiff, levelsA[2], 128, false, 255, 1));
        ASSERT_NO_FATAL_FAILURE(writeTiledDirectory(tiff, levelsB[0], 128, false, 255, 0));
        ASSERT_NO_FATAL_FAILURE(writeTiledDirectory(tiff, levelsB[1], 128, false, 255, 1));
    });
    ASSERT_FALSE(::testing::Test::HasFatalFailure());

    auto slide = openGdal(path);
    ASSERT_EQ(2, slide->getNumScenes());
    auto sceneA = slide->getScene(0);
    auto sceneB = slide->getScene(1);
    EXPECT_EQ(cv::Rect(0, 0, 1000, 800), sceneA->getRect());
    EXPECT_EQ(cv::Rect(0, 0, 600, 600), sceneB->getRect());
    {
        SCOPED_TRACE("scene A");
        expectLevelInfo(sceneA, levelsA, {{128, 128}, {128, 128}, {128, 128}});
        expectLevelsRead(sceneA, levelsA);
    }
    {
        SCOPED_TRACE("scene B");
        expectLevelInfo(sceneB, levelsB, {{128, 128}, {128, 128}});
        // Level 1 of B is directory 4.
        expectLevelsRead(sceneB, levelsB);
    }
}

// A reduced page whose channel count differs from the open base is not a level:
// it becomes a scene of its own, one level, one channel.
TEST(GDALTiffPyramid, mismatchedReducedPageStaysOwnScene)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    const auto levels = makePyramid(makeRgbImage(1000, 800), {{500, 400}});
    cv::Mat gray;
    cv::extractChannel(makeRgbImage(250, 200, 50), gray, 2);
    writeTiff(path, [&](libtiff::TIFF* tiff) {
        ASSERT_NO_FATAL_FAILURE(writeTiledDirectory(tiff, levels[0], 128, false, 255, 0));
        ASSERT_NO_FATAL_FAILURE(writeTiledDirectory(tiff, levels[1], 128, false, 255, 1));
        ASSERT_NO_FATAL_FAILURE(writeTiledDirectory(tiff, gray, 128, false, 255, 1));
    });
    ASSERT_FALSE(::testing::Test::HasFatalFailure());

    auto slide = openGdal(path);
    ASSERT_EQ(2, slide->getNumScenes());
    {
        SCOPED_TRACE("scene 0");
        auto scene = slide->getScene(0);
        EXPECT_EQ(3, scene->getNumChannels());
        expectLevelsRead(scene, levels);
    }
    {
        SCOPED_TRACE("scene 1");
        auto scene = slide->getScene(1);
        EXPECT_EQ(cv::Rect(0, 0, 250, 200), scene->getRect());
        EXPECT_EQ(1, scene->getNumChannels());
        expectLevelsRead(scene, {gray});
    }
}

// Rule 6: a reduced page that is not a level of the base starts a scene of its own
// and carries the reduced pages that match it. A (RGB), g1 and g2 (gray, both
// flagged reduced) open as A alone and g1 with g2 as its level.
TEST(GDALTiffPyramid, mismatchedReducedPageFileOpensWithItsOwnLevels)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    const cv::Mat base = makeRgbImage(1000, 800);
    cv::Mat gray;
    cv::extractChannel(makeRgbImage(500, 400, 50), gray, 2);
    const auto grayLevels = makePyramid(gray, {{250, 200}});
    writeTiff(path, [&](libtiff::TIFF* tiff) {
        ASSERT_NO_FATAL_FAILURE(writeTiledDirectory(tiff, base, 128, false, 255, 0));
        ASSERT_NO_FATAL_FAILURE(writeTiledDirectory(tiff, grayLevels[0], 128, false, 255, 1));
        ASSERT_NO_FATAL_FAILURE(writeTiledDirectory(tiff, grayLevels[1], 128, false, 255, 1));
    });
    ASSERT_FALSE(::testing::Test::HasFatalFailure());

    auto slide = openGdal(path);
    ASSERT_EQ(2, slide->getNumScenes());
    {
        SCOPED_TRACE("scene 0");
        auto scene = slide->getScene(0);
        EXPECT_EQ(3, scene->getNumChannels());
        expectLevelsRead(scene, {base});
    }
    {
        SCOPED_TRACE("scene 1");
        auto scene = slide->getScene(1);
        EXPECT_EQ(cv::Rect(0, 0, 500, 400), scene->getRect());
        EXPECT_EQ(1, scene->getNumChannels());
        expectLevelInfo(scene, grayLevels, {{128, 128}, {128, 128}});
        expectLevelsRead(scene, grayLevels);
    }
}

// The scene holds no handle of the slide's: it stays readable, at every level,
// once the slide is gone.
TEST(GDALTiffPyramid, pyramidSceneOutlivesSlide)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    const auto levels = makePyramid(makeRgbImage(1000, 800), kSubIfdLevels);
    ASSERT_NO_FATAL_FAILURE(writeSubIfdPyramid(path, levels, 256));

    auto slide = openGdal(path);
    auto scene = slide->getScene(0);
    slide.reset();
    expectLevelsRead(scene, levels);
}

// Every handle the scenes read through is closed once the last scene and the
// slide are gone: on Windows an open handle would make the remove fail.
TEST(GDALTiffPyramid, fileIsReleasedAfterSceneAndSlideAreGone)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    const auto levels = makePyramid(makeRgbImage(1000, 800), kSubIfdLevels);
    ASSERT_NO_FATAL_FAILURE(writeSubIfdPyramid(path, levels, 256));

    {
        auto slide = openGdal(path);
        auto scene = slide->getScene(0);
        expectLevelsRead(scene, levels);
        // Concurrent reads, so the pool holds more than one handle.
        expectConcurrentReadsMatchSerial(scene);
        slide.reset();
        scene.reset();
    }
    std::error_code error;
    EXPECT_TRUE(std::filesystem::remove(temp.getPath(), error)) << error.message();
    EXPECT_FALSE(std::filesystem::exists(temp.getPath()));
    // TempFile's destructor checks exists() first, so the file being gone is fine.
}

// No vendor driver claims a generic pyramidal tiff, so auto-detection lands in GDAL.
TEST(GDALTiffPyramid, autoDetectionLandsInGdal)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    const auto levels = makePyramid(makeRgbImage(1000, 800), kSubIfdLevels);
    ASSERT_NO_FATAL_FAILURE(writeSubIfdPyramid(path, levels, 256));

    auto slide = slideio::openSlide(path, "");
    ASSERT_TRUE(slide != nullptr);
    EXPECT_EQ("GDAL", slide->getDriverId());
    ASSERT_EQ(1, slide->getNumScenes());
    EXPECT_EQ(3, slide->getScene(0)->getNumZoomLevels());
}

TEST(GDALTiffPyramid, tiffSceneReportsConcurrent)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    const auto levels = makePyramid(makeRgbImage(1000, 800), kSubIfdLevels);
    ASSERT_NO_FATAL_FAILURE(writeSubIfdPyramid(path, levels, 256));

    auto slide = openGdal(path);
    EXPECT_TRUE(slide->getScene(0)->supportsConcurrentReads());
}

TEST(GDALTiffPyramid, concurrentRandomReadsMatchSerial)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    const auto levels = makePyramid(makeRgbImage(1000, 800), kSubIfdLevels);
    ASSERT_NO_FATAL_FAILURE(writeSubIfdPyramid(path, levels, 256));

    auto slide = openGdal(path);
    expectConcurrentReadsMatchSerial(slide->getScene(0));
}

// A plain many-page tiff read page by page must not hold a handle per page: every
// scene of the file borrows from one pool, bounded by ContextPool::defaultMax().
TEST(GDALTiffPyramid, manyPagesShareBoundedHandles)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    constexpr int kPages = 64;
    std::vector<cv::Mat> pages;
    for (int page = 0; page < kPages; ++page) {
        pages.push_back(makeRgbImage(96, 80, page));
    }
    writeTiff(path, [&](libtiff::TIFF* tiff) {
        for (const cv::Mat& page : pages) {
            ASSERT_NO_FATAL_FAILURE(writeTiledDirectory(tiff, page, 64, false, 255));
        }
    });
    ASSERT_FALSE(::testing::Test::HasFatalFailure());

    auto slide = openGdal(path);
    ASSERT_EQ(kPages, slide->getNumScenes());
    std::set<const slideio::ContextPool*> pools;
    for (int index = 0; index < kPages; ++index) {
        SCOPED_TRACE(::testing::Message() << "scene " << index);
        auto scene = std::dynamic_pointer_cast<slideio::GDALTiffScene>(slide->getScene(index));
        ASSERT_TRUE(scene != nullptr);
        ASSERT_TRUE(scene->contextPool() != nullptr);
        pools.insert(scene->contextPool());
        cv::Mat block;
        scene->readBlock({10, 10, 50, 40}, block);
        EXPECT_EQ(0, cv::norm(pages[index]({10, 10, 50, 40}), block, cv::NORM_INF));
    }
    EXPECT_EQ(1u, pools.size());
    // Every handle the file's scenes opened, wherever they live.
    int handles = 0;
    for (const slideio::ContextPool* pool : pools) {
        handles += pool->contextCount();
    }
    EXPECT_LE(handles, slideio::ContextPool::defaultMax());
}

// A striped base with tiled levels, read through readResampledBlock with a scale,
// so level selection lands on the striped base for some reads and the tiled level
// for others.
TEST(GDALTiffPyramid, concurrentStripedScaledReadsMatchSerial)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    const auto levels = makePyramid(makeRgbImage(600, 400), {{300, 200}, {150, 100}});
    writeTiff(path, [&](libtiff::TIFF* tiff) {
        ASSERT_NO_FATAL_FAILURE(writeStripedDirectory(tiff, levels[0], 0, 2));
        ASSERT_NO_FATAL_FAILURE(writeTiledDirectory(tiff, levels[1], 64, false, 255, 1));
        ASSERT_NO_FATAL_FAILURE(writeTiledDirectory(tiff, levels[2], 64, false, 255, 1));
    });
    ASSERT_FALSE(::testing::Test::HasFatalFailure());

    auto slide = openGdal(path);
    ASSERT_EQ(1, slide->getNumScenes());
    auto scene = slide->getScene(0);
    ASSERT_EQ(3, scene->getNumZoomLevels());
    expectConcurrentReadsMatchSerial(scene, ReadPath::ScaledBlock);
}

// A striped level is decoded whole on every read, so concurrent reads of striped
// levels would hold one whole level each in memory. They are serialised per file:
// two striped pages (two scenes) of one file, 8 threads reading both, and at no
// point is more than one whole-level decode in flight. The counter is maintained
// inside the driver, so the assertion does not depend on timing; without the lock
// 8 threads decoding a 2000x1500 level overlap on every run.
TEST(GDALTiffPyramid, stripedDecodesDoNotOverlap)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    const cv::Mat page0 = makeRgbImage(2000, 1500, 0);
    const cv::Mat page1 = makeRgbImage(2000, 1500, 100);
    writeTiff(path, [&](libtiff::TIFF* tiff) {
        ASSERT_NO_FATAL_FAILURE(writeStripedDirectory(tiff, page0));
        ASSERT_NO_FATAL_FAILURE(writeStripedDirectory(tiff, page1));
    });
    ASSERT_FALSE(::testing::Test::HasFatalFailure());

    auto slide = openGdal(path);
    ASSERT_EQ(2, slide->getNumScenes());
    const std::vector<std::shared_ptr<slideio::CVScene>> scenes{slide->getScene(0), slide->getScene(1)};
    const std::vector<cv::Mat> pages{page0, page1};

    slideio::GDALTiffScene::resetStripedDecodeStats();
    constexpr int kThreads = 8;
    constexpr int kReads = 4;
    std::atomic<int> mismatches{0};
    std::atomic<int> failures{0};
    auto reader = [&](int t) {
        try {
            const int index = t % 2;
            for (int read = 0; read < kReads; ++read) {
                const cv::Rect rect(100 * t + 10 * read, 50 * read, 300, 200);
                cv::Mat block;
                scenes[index]->readBlock(rect, block);
                if (block.size() != rect.size() || cv::norm(pages[index](rect), block, cv::NORM_INF) != 0) {
                    ++mismatches;
                }
            }
        }
        catch (...) {
            ++failures;
        }
    };
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back(reader, t);
    }
    for (auto& thread : threads) {
        thread.join();
    }
    EXPECT_EQ(0, failures.load());
    EXPECT_EQ(0, mismatches.load());
    EXPECT_EQ(1, slideio::GDALTiffScene::maxStripedDecodesInFlight());
}

TEST(GDALTiffPyramid, concurrentFreeImageReadsMatchSerial)
{
    slideio::TempFile temp("png");
    const std::string path = temp.getPath().string();
    slideio::ImageTools::writeSmallImageRaster(path, slideio::Compression::Png, makeRgbImage(1200, 900));

    auto slide = openGdal(path);
    auto scene = slide->getScene(0);
    ASSERT_EQ(1, scene->getNumZoomLevels());
    expectConcurrentReadsMatchSerial(scene);
}
