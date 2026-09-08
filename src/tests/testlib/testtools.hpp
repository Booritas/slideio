// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <gtest/gtest.h>   // for GTEST_SKIP in the macros at the foot of this file
#include <string>
#include <opencv2/core.hpp>

namespace slideio
{
    class ImageDriver;
    class CVScene;
    class CVSlide;
}

class TestTools
{
public:
	static bool isPrivateTestEnabled();
    static bool isFullTestEnabled();
    static std::string getTestImageDirectory(bool priv=false);
    static std::string getTestImagePath(const std::string& subfolder, const std::string& image);
    // True while some handle in this process still holds `path` open, so that a test
    // can observe a handle being closed rather than assume it. See the .cpp for the
    // platform split, and for why this is not an attempt to delete the file.
    static bool isFileHeldOpen(const std::string& path);
    // True when the run asked for absent test images to be skipped rather than
    // failed, i.e. SLIDEIO_SKIP_MISSING_IMAGES is set to anything but 0. Read
    // once and cached. See the macros below.
    static bool skipsMissingImages();
    // True when `path` exists. Works for a directory as well as a file.
    static bool imageExists(const std::string& path);
    // True when SLIDEIO_IMAGES_PATH/<subfolder> exists.
    static bool imageDirExists(const std::string& subfolder);
    static void readRawImage(const std::string& path, cv::Mat& image);
    static void writeRawImage(const std::string& path, const cv::Mat& image);
    static void compareRasters(cv::Mat& raster1, cv::Mat& raster2);
    static bool compareRastersEx(cv::Mat& raster1, cv::Mat& raster2);
    static bool isRasterEmpty(cv::Mat& raster);
    static void showRaster(cv::Mat& raster);
    static void showResampledRaster(cv::Mat& raster, int maxSize=800);
    static void showRasters(cv::Mat& raster1, cv::Mat& raster2);
    static void writePNG(cv::Mat raster, const std::string& filePath);
    static void readPNG(const std::string& filePath, cv::OutputArray output);
    static void readTiffDirectory(const std::string& filePath, int dir, cv::OutputArray output);
    static void readTiffDirectories(const std::string& filePath, const std::vector<int>& dirIndices, cv::OutputArray output);
    static size_t countNonZero(const cv::Mat& mat);
    static bool starts_with(const std::string& str, const std::string& prefix) {
        return str.size() >= prefix.size() &&
           std::equal(prefix.begin(), prefix.end(), str.begin());
    }
    static void multiThreadedTest(const std::string& filePath, slideio::ImageDriver& driver, int numberRois = 5, int numThreads = 30);
    // Reads a set of ROIs single-threaded to build a baseline, then reads every
    // ROI from numThreads threads and requires every result to be byte-identical
    // to its baseline.
    //
    // This is the gate for concurrent reads, and it is shaped by the failure it
    // has to catch: a race on a shared decode buffer or file cursor produces
    // WRONG PIXELS, not an exception. A test that only checks for absent
    // exceptions passes while the data is corrupt.
    // Which entry shape the reads go through. The default AllChannels was for a
    // while the only one covered, which left the most per-read-state-heavy code
    // on the concurrency branch untested: PKE resolves a *different directory
    // per channel* and SCN walks channel2ifd per channel, so a channel subset
    // touches per-read state that an all-channels read never reaches, and the
    // level-addressed path is a separate entry point with its own lock.
    enum class ConcurrentReadPath
    {
        AllChannels,    // readResampledBlockChannels with an empty channel list
        SingleChannel,  // one explicit channel, the last one
        ChannelSubset,  // an explicit list of more than one channel, out of order
        Level           // readResampledLevelBlockChannels at level 0
    };

    static void concurrentReadIdentityTest(const std::string& filePath,
                                           slideio::ImageDriver& driver,
                                           int sceneIndex = 0,
                                           int numRois = 8,
                                           int numThreads = 16,
                                           int readsPerThread = 8,
                                           ConcurrentReadPath path = ConcurrentReadPath::AllChannels);

    // Runs concurrentReadIdentityTest once per entry shape. Spec 6 requires
    // channel subsets and the level-addressed path as well as the 2D
    // all-channels read; this is how a driver covers all of them in one call.
    static void concurrentReadIdentityTestAllPaths(const std::string& filePath,
                                                   slideio::ImageDriver& driver,
                                                   int sceneIndex = 0,
                                                   int numRois = 4,
                                                   int numThreads = 16,
                                                   int readsPerThread = 4);

    // The same, applied to every scene of the slide in turn. Formats whose
    // slides carry more than one kind of scene -- VSI has ETS scenes and TIFF
    // scenes -- need every kind covered, not just scene 0.
    static void concurrentReadIdentityTestAllScenes(const std::string& filePath,
                                                    slideio::ImageDriver& driver,
                                                    int numRois = 4,
                                                    int numThreads = 16,
                                                    int readsPerThread = 4);
    static std::shared_ptr<slideio::CVScene> findScene(std::shared_ptr<slideio::CVSlide> slide, const std::string& name);
};

// The image corpus does not fit on every machine, so directories get rotated in
// and out. Without these, a rotated-out directory turns a suite entirely red and
// a real regression is indistinguishable from an absent file -- which has already
// caused one bug to be filed as "missing data" and one absent directory to be
// reported as a regression.
//
// Skipping is OPT-IN: with SLIDEIO_SKIP_MISSING_IMAGES unset, as on CI, a missing
// image fails exactly as it did before, so coverage cannot quietly disappear.
// Set it locally and the run reads "3 failed, 40 skipped" instead of "43 failed".
//
// Both expand to a `return`, so they must sit in the test body itself or in a
// fixture's SetUp -- not in a helper function, where the return would only leave
// the helper and the test would carry on into the missing file.
#define SLIDEIO_SKIP_IF_IMAGE_MISSING(path)                                     \
    do {                                                                        \
        const std::string slideioSkipPath__ = (path);                           \
        /* order matters: with skipping off this costs nothing, no stat call */ \
        if (TestTools::skipsMissingImages()                                     \
            && !TestTools::imageExists(slideioSkipPath__)) {                    \
            GTEST_SKIP() << "test image is not present: " << slideioSkipPath__; \
        }                                                                       \
    } while (false)

#define SLIDEIO_SKIP_IF_IMAGE_DIR_MISSING(subfolder)                            \
    do {                                                                        \
        const std::string slideioSkipDir__ = (subfolder);                       \
        if (TestTools::skipsMissingImages()                                     \
            && !TestTools::imageDirExists(slideioSkipDir__)) {                  \
            GTEST_SKIP() << "test image directory is not present: "             \
                         << slideioSkipDir__;                                   \
        }                                                                       \
    } while (false)
