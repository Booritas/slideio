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
	static std::string getTestImagePath(const std::string& subfolder, const std::string& image, bool priv=false);
    static std::string getFullTestImagePath(const std::string& subfolder, const std::string& image);
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
