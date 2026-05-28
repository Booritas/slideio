// src/tests/main/test_converter_s3.cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// End-to-end converter tests with input read over HTTP. Exercises the v1 S3
// streaming foundation through slideio_converter's convertFile entry point
// (the same function the CLI calls), with the local HttpFixture serving a
// small SVS test slide. One single-threaded case here; the multi-threaded
// case (cloneScene reopen over HTTP) is added in a later task.

#include "tests/testlib/testtools.hpp"
#include "tools/converter/sceneconverter.hpp"
#include "http_fixture/http_fixture.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/base/rect.hpp"
#include "slideio/base/range.hpp"

#include <gtest/gtest.h>
#include <filesystem>
#include <opencv2/imgproc.hpp>

namespace {

// Copies a test image into a clean temp directory that HttpFixture can serve.
// The destination filename can be sanitized to avoid percent-encoding issues
// in the served URL.
std::filesystem::path stageImage(const std::string& srcPath,
                                 const std::string& tag,
                                 const std::string& destName)
{
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / ("slideio_conv_s3_" + tag);
    fs::remove_all(root);
    fs::create_directories(root);
    fs::copy_file(srcPath, root / destName, fs::copy_options::overwrite_existing);
    return root;
}

// Build a clean output path under a fresh temp subdirectory and ensure no
// stale output remains. Returns the path; caller is responsible for cleanup
// if needed (test infrastructure tears down the parent on next run).
std::filesystem::path freshOutputPath(const std::string& tag, const std::string& name)
{
    namespace fs = std::filesystem;
    fs::path out = fs::temp_directory_path() / ("slideio_conv_s3_out_" + tag);
    fs::remove_all(out);
    fs::create_directories(out);
    return out / name;
}

// Convert with the converter tool's convertFile() helper. All optional flags
// defaulted to the values that match a typical slideio_converter invocation;
// only the input, output, and thread counts vary per test.
void runConvertFile(const std::string& inputPath,
                    const std::string& outputPath,
                    int numReadingThreads,
                    int numEncodingThreads)
{
    slideio::Rect emptyRect;
    slideio::Range emptyChannelRange;
    slideio::Range emptySliceRange;
    slideio::Range emptyFrameRange;
    convertFile(
        inputPath,
        outputPath,
        /*sceneIndex=*/0,
        /*compressionRate=*/5.0,
        /*tileSize=*/512,
        /*numZoomLevels=*/-1,
        /*inputDriver=*/"AUTO",
        /*targetFormat=*/"SVS",
        /*targetCompression=*/"Jpeg",
        /*compressionQuality=*/85,
        emptyRect,
        emptyChannelRange,
        emptySliceRange,
        emptyFrameRange,
        /*silent=*/true,
        /*infoOnly=*/false,
        /*deleteIfExists=*/true,
        /*tileBatchSize=*/10,
        numReadingThreads,
        numEncodingThreads);
}

// Compare two converted slides: dimensions, channels, and a sampled
// top-left tile read of equal bytes. We do *not* byte-compare the SVS files
// because the SVS image-description tag embeds a UUID and timestamps.
void expectConvertedSlidesMatch(const std::string& pathA,
                                const std::string& pathB)
{
    auto slideA = slideio::ImageDriverManager::openSlide(pathA, "SVS");
    auto slideB = slideio::ImageDriverManager::openSlide(pathB, "SVS");
    ASSERT_TRUE(slideA);
    ASSERT_TRUE(slideB);
    ASSERT_EQ(slideA->getNumScenes(), slideB->getNumScenes());
    ASSERT_GT(slideA->getNumScenes(), 0);
    auto sceneA = slideA->getScene(0);
    auto sceneB = slideB->getScene(0);
    ASSERT_TRUE(sceneA);
    ASSERT_TRUE(sceneB);
    EXPECT_EQ(sceneA->getRect(), sceneB->getRect());
    EXPECT_EQ(sceneA->getNumChannels(), sceneB->getNumChannels());

    const cv::Rect rect = sceneA->getRect();
    cv::Rect block(0, 0, std::min(256, rect.width), std::min(256, rect.height));
    cv::Mat rasterA, rasterB;
    sceneA->readBlock(block, rasterA);
    sceneB->readBlock(block, rasterB);
    ASSERT_EQ(rasterA.size(), rasterB.size());
    EXPECT_EQ(0.0, cv::norm(rasterA, rasterB, cv::NORM_INF));
}

} // namespace

// Test 1: single-threaded converter over HTTP input.
// Stages CMU-1-Small-Region.svs to a temp dir, serves it over the local
// HttpFixture, converts it to a local SVS via convertFile, then converts the
// same slide from the local path, and checks the two outputs match.
TEST(ConverterS3, SingleThreaded_HttpInput_MatchesLocal)
{
    namespace fs = std::filesystem;
    const std::string name = "CMU-1-Small-Region.svs";
    const std::string localSrc = TestTools::getTestImagePath("svs", name);
    fs::path root = stageImage(localSrc, "st", name);

    slideio::tests::HttpFixture fx(root);
    const std::string httpUrl = fx.url(name);

    fs::path outFromHttp = freshOutputPath("st_http", "out.svs");
    fs::path outFromLocal = freshOutputPath("st_local", "out.svs");

    ASSERT_NO_THROW(runConvertFile(httpUrl, outFromHttp.generic_string(), 1, 1));
    ASSERT_NO_THROW(runConvertFile((root / name).generic_string(), outFromLocal.generic_string(), 1, 1));

    expectConvertedSlidesMatch(outFromHttp.generic_string(), outFromLocal.generic_string());
}

// Test 2: multi-threaded converter over HTTP input.
// Exercises TiffConverter::cloneScene() reopening the stream-opened slide
// once per reader thread. Each reader opens its own HttpStream + cache.
TEST(ConverterS3, MultiThreaded_HttpInput_MatchesLocal)
{
    namespace fs = std::filesystem;
    const std::string name = "CMU-1-Small-Region.svs";
    const std::string localSrc = TestTools::getTestImagePath("svs", name);
    fs::path root = stageImage(localSrc, "mt", name);

    slideio::tests::HttpFixture fx(root);
    const std::string httpUrl = fx.url(name);

    fs::path outFromHttp = freshOutputPath("mt_http", "out.svs");
    fs::path outFromLocal = freshOutputPath("mt_local", "out.svs");

    // 4 reader + 4 encoder threads forces TiffConverter into its MT path
    // (cloneScene() + per-reader HttpStream).
    ASSERT_NO_THROW(runConvertFile(httpUrl, outFromHttp.generic_string(), 4, 4));
    ASSERT_NO_THROW(runConvertFile((root / name).generic_string(), outFromLocal.generic_string(), 4, 4));

    expectConvertedSlidesMatch(outFromHttp.generic_string(), outFromLocal.generic_string());
}
