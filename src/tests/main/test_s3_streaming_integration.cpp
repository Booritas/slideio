// src/tests/main/test_s3_streaming_integration.cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// Phase J: end-to-end integration tests that exercise real driver opens over the
// actual HttpStream path (ranged GETs + block cache), not just an in-process
// MemoryStream. AFI-over-HTTP (J4) lives in test_afi_driver.cpp; this file adds
// SVS and NDPI over HTTP plus the two gaps reviewers flagged:
//   * AUTO driver detection over an http URI (Phase H review)
//   * SVS aux-image lazy reopen FROM THE STREAM (SVS migration review)

#include "tests/testlib/testtools.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/core/cvscene.hpp"
#include "http_fixture/http_fixture.hpp"

#include <gtest/gtest.h>
#include <filesystem>
#include <algorithm>
#include <opencv2/imgproc.hpp>

namespace {

// Copies a source test image into a fresh, isolated temp directory that can be
// served by HttpFixture, and returns that directory. The destination filename
// can be sanitized (no spaces/parens) so the served URL stays free of
// characters the test server / curl would otherwise have to percent-encode.
std::filesystem::path stageImage(const std::string& srcPath,
                                 const std::string& tag,
                                 const std::string& destName)
{
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / ("slideio_s3_" + tag);
    fs::remove_all(root);
    fs::create_directories(root);
    fs::copy_file(srcPath, root / destName, fs::copy_options::overwrite_existing);
    return root;
}

} // namespace

// Test 1: SVS over HTTP -- named driver, scene-0 region parity.
TEST(S3StreamingIntegration, SvsOverHttpNamedDriver)
{
    namespace fs = std::filesystem;
    const std::string name = "CMU-1-Small-Region.svs";
    const std::string localPath = TestTools::getTestImagePath("svs", name);
    fs::path root = stageImage(localPath, "svs_named", name);

    auto localSlide = slideio::ImageDriverManager::openSlide((root / name).generic_string(), "SVS");
    ASSERT_TRUE(localSlide);

    slideio::tests::HttpFixture fx(root);
    auto httpSlide = slideio::ImageDriverManager::openSlide(fx.url(name), "SVS");
    ASSERT_TRUE(httpSlide);

    ASSERT_EQ(localSlide->getNumScenes(), httpSlide->getNumScenes());
    ASSERT_GT(localSlide->getNumScenes(), 0);

    auto localScene = localSlide->getScene(0);
    auto httpScene = httpSlide->getScene(0);
    ASSERT_TRUE(localScene);
    ASSERT_TRUE(httpScene);
    EXPECT_EQ(localScene->getRect(), httpScene->getRect());
    EXPECT_EQ(localScene->getNumChannels(), httpScene->getNumChannels());

    const cv::Rect sceneRect = localScene->getRect();
    cv::Rect block(0, 0, std::min(256, sceneRect.width), std::min(256, sceneRect.height));
    cv::Mat localRaster, httpRaster;
    localScene->readBlock(block, localRaster);
    httpScene->readBlock(block, httpRaster);
    ASSERT_EQ(localRaster.size(), httpRaster.size());
    EXPECT_EQ(0.0, cv::norm(localRaster, httpRaster, cv::NORM_INF));
}

// Test 2: SVS over HTTP -- AUTO driver detection.
// Opening with an empty driver name routes through findDriver -> canOpenFile ->
// matchPattern, which must strip the query string and detect SVS over an http
// URI. This is the gap flagged in the Phase H review.
TEST(S3StreamingIntegration, SvsOverHttpAutoDetection)
{
    namespace fs = std::filesystem;
    const std::string name = "CMU-1-Small-Region.svs";
    const std::string localPath = TestTools::getTestImagePath("svs", name);
    fs::path root = stageImage(localPath, "svs_auto", name);

    auto localSlide = slideio::ImageDriverManager::openSlide((root / name).generic_string(), "AUTO");
    ASSERT_TRUE(localSlide);

    slideio::tests::HttpFixture fx(root);
    // Empty driver name == AUTO selection.
    auto httpSlide = slideio::ImageDriverManager::openSlide(fx.url(name), "");
    ASSERT_TRUE(httpSlide);
    EXPECT_EQ("SVS", httpSlide->getDriverId());

    ASSERT_EQ(localSlide->getNumScenes(), httpSlide->getNumScenes());
    ASSERT_GT(httpSlide->getNumScenes(), 0);

    auto localScene = localSlide->getScene(0);
    auto httpScene = httpSlide->getScene(0);
    ASSERT_TRUE(httpScene);
    EXPECT_EQ(localScene->getRect(), httpScene->getRect());

    const cv::Rect sceneRect = httpScene->getRect();
    cv::Rect block(0, 0, std::min(256, sceneRect.width), std::min(256, sceneRect.height));
    cv::Mat localRaster, httpRaster;
    localScene->readBlock(block, localRaster);
    httpScene->readBlock(block, httpRaster);
    ASSERT_EQ(localRaster.size(), httpRaster.size());
    EXPECT_EQ(0.0, cv::norm(localRaster, httpRaster, cv::NORM_INF));
}

// Test 3: SVS over HTTP -- aux-image read (lazy reopen FROM THE STREAM).
// SVS aux images (Thumbnail/Label/Macro) reopen lazily via makeSureFileIsOpened().
// For a stream-opened slide they must reopen from the stream, not from a path.
// Reading the aux raster over http and comparing byte-for-byte against the local
// open exercises that reopen branch.
TEST(S3StreamingIntegration, SvsOverHttpAuxImageReopenFromStream)
{
    namespace fs = std::filesystem;
    const std::string name = "CMU-1-Small-Region.svs";
    const std::string localPath = TestTools::getTestImagePath("svs", name);
    fs::path root = stageImage(localPath, "svs_aux", name);

    auto localSlide = slideio::ImageDriverManager::openSlide((root / name).generic_string(), "SVS");
    ASSERT_TRUE(localSlide);

    slideio::tests::HttpFixture fx(root);
    auto httpSlide = slideio::ImageDriverManager::openSlide(fx.url(name), "SVS");
    ASSERT_TRUE(httpSlide);

    // Pick an aux image present in both slides (CMU-1-Small-Region exposes
    // Thumbnail/Label/Macro; Thumbnail is reliably present).
    const std::list<std::string>& auxNames = localSlide->getAuxImageNames();
    ASSERT_FALSE(auxNames.empty());
    std::string auxName = "Thumbnail";
    if (std::find(auxNames.begin(), auxNames.end(), auxName) == auxNames.end()) {
        auxName = auxNames.front();
    }

    const std::list<std::string>& httpAuxNames = httpSlide->getAuxImageNames();
    ASSERT_TRUE(std::find(httpAuxNames.begin(), httpAuxNames.end(), auxName) != httpAuxNames.end())
        << "aux image '" << auxName << "' missing from http-opened slide";

    auto localAux = localSlide->getAuxImage(auxName);
    auto httpAux = httpSlide->getAuxImage(auxName);
    ASSERT_TRUE(localAux);
    ASSERT_TRUE(httpAux);
    EXPECT_EQ(localAux->getRect(), httpAux->getRect());
    EXPECT_EQ(localAux->getNumChannels(), httpAux->getNumChannels());

    // Read the full (small) aux raster from both. The http read forces the aux
    // scene's makeSureFileIsOpened() to reopen the underlying TIFF from the
    // stream; if it tried to reopen from a path it would fail (the http URI is
    // not a local file).
    const cv::Rect rect = localAux->getRect();
    cv::Mat localRaster, httpRaster;
    localAux->readBlock(rect, localRaster);
    httpAux->readBlock(rect, httpRaster);
    ASSERT_EQ(localRaster.size(), httpRaster.size());
    EXPECT_EQ(0.0, cv::norm(localRaster, httpRaster, cv::NORM_INF));
}

// Test 4: NDPI over HTTP -- JPEG/MCU path over real ranged GETs.
// Validates the NDPIDataSource + libjpeg-over-stream path against real
// HttpStream ranged GETs (the unit test used MemoryStream). The NDPI driver is
// linked into the main slideio lib, so openSlide(..., "NDPI") works here.
TEST(S3StreamingIntegration, NdpiOverHttpNamedDriver)
{
    namespace fs = std::filesystem;
    const std::string srcName = "test3-DAPI-2-(387).ndpi";
    const std::string localPath = TestTools::getTestImagePath("ndpi", srcName);
    if (!fs::exists(localPath)) {
        GTEST_SKIP() << "NDPI test image not available: " << localPath;
    }
    // Sanitized served name: drop spaces/parens so the URL needs no encoding.
    const std::string servedName = "test3_DAPI_2_387.ndpi";
    fs::path root = stageImage(localPath, "ndpi_named", servedName);

    auto localSlide = slideio::ImageDriverManager::openSlide((root / servedName).generic_string(), "NDPI");
    ASSERT_TRUE(localSlide);

    slideio::tests::HttpFixture fx(root);
    auto httpSlide = slideio::ImageDriverManager::openSlide(fx.url(servedName), "NDPI");
    ASSERT_TRUE(httpSlide);

    ASSERT_EQ(localSlide->getNumScenes(), httpSlide->getNumScenes());
    ASSERT_GT(localSlide->getNumScenes(), 0);

    auto localScene = localSlide->getScene(0);
    auto httpScene = httpSlide->getScene(0);
    ASSERT_TRUE(localScene);
    ASSERT_TRUE(httpScene);
    EXPECT_EQ(localScene->getRect(), httpScene->getRect());
    EXPECT_EQ(localScene->getNumChannels(), httpScene->getNumChannels());

    const cv::Rect sceneRect = localScene->getRect();
    cv::Rect block(0, 0, std::min(256, sceneRect.width), std::min(256, sceneRect.height));
    cv::Mat localRaster, httpRaster;
    localScene->readBlock(block, localRaster);
    httpScene->readBlock(block, httpRaster);
    ASSERT_EQ(localRaster.size(), httpRaster.size());
    EXPECT_EQ(0.0, cv::norm(localRaster, httpRaster, cv::NORM_INF));
}
