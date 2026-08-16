// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <opencv2/core.hpp>
#include "tests/testlib/testscene.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/core/levelinfo.hpp"
#include "slideio/slideio/slideio.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/slideio/slide.hpp"
#include "tests/testlib/testtools.hpp"

using namespace slideio;

namespace
{
    // A 100x100 byte scene with a two level pyramid: level 0 at full size, level 1 at half.
    // Deterministic and file-free, so these tests exercise the contract rather than a driver.
    std::shared_ptr<TestScene> makeTwoLevelScene()
    {
        auto scene = std::make_shared<TestScene>();
        scene->setRect(cv::Rect(0, 0, 100, 100));
        scene->setNumChannels(3);
        scene->setChannelDataType(DataType::DT_Byte);
        scene->setRenderCoordinates(true);
        scene->addLevel(LevelInfo(0, {100, 100}, 1.0, 20., {100, 100}));
        scene->addLevel(LevelInfo(1, {50, 50}, 0.5, 10., {50, 50}));
        return scene;
    }
}

TEST(LevelReadingTests, rejectsAnOutOfRangeLevel)
{
    auto scene = makeTwoLevelScene();
    cv::Mat raster;
    EXPECT_THROW(scene->readResampledLevelBlockChannels(-1, cv::Rect(0, 0, 10, 10), cv::Size(10, 10), {}, raster),
                 RuntimeError);
    EXPECT_THROW(scene->readResampledLevelBlockChannels(2, cv::Rect(0, 0, 10, 10), cv::Size(10, 10), {}, raster),
                 RuntimeError);
}

TEST(LevelReadingTests, rejectsASceneWithoutLevels)
{
    auto scene = makeTwoLevelScene();
    scene->clearLevels();
    cv::Mat raster;
    EXPECT_THROW(scene->readResampledLevelBlockChannels(0, cv::Rect(0, 0, 10, 10), cv::Size(10, 10), {}, raster),
                 RuntimeError);
}

// Level 0 has scale 1, so the default implementation must pass the rectangle through
// untouched. Anything else would mean the identity case is not identity.
TEST(LevelReadingTests, defaultImplementationPassesLevelZeroThrough)
{
    auto scene = makeTwoLevelScene();
    cv::Mat raster;
    scene->readResampledLevelBlockChannels(0, cv::Rect(10, 20, 30, 40), cv::Size(30, 40), {}, raster);
    ASSERT_EQ(1u, scene->requests().size());
    EXPECT_EQ(cv::Rect(10, 20, 30, 40), scene->requests()[0].rect);
    EXPECT_EQ(cv::Size(30, 40), scene->requests()[0].size);
    EXPECT_EQ(cv::Size(30, 40), raster.size());
    EXPECT_EQ(CV_8UC3, raster.type());
}

// Level 1 has scale 0.5, so a level-1 rectangle at (10,10) covers scene pixels from (20,20).
// This is the conversion the caller no longer has to write, done once and in one place.
TEST(LevelReadingTests, defaultImplementationMapsADeeperLevelOntoSceneCoordinates)
{
    auto scene = makeTwoLevelScene();
    cv::Mat raster;
    scene->readResampledLevelBlockChannels(1, cv::Rect(10, 10, 20, 20), cv::Size(20, 20), {}, raster);
    ASSERT_EQ(1u, scene->requests().size());
    EXPECT_EQ(cv::Rect(20, 20, 40, 40), scene->requests()[0].rect);
    EXPECT_EQ(cv::Size(20, 20), scene->requests()[0].size);
    EXPECT_EQ(cv::Size(20, 20), raster.size());
}

// The right and bottom edge tiles of any level overhang it. The caller gets an array of the
// size it asked for, the in-bounds part carries data, and the rest is background -- so every
// tile in a grid has the same shape and a viewer's cache does not need a special case.
TEST(LevelReadingTests, defaultImplementationClampsAndBackgroundFillsAnOverhangingRect)
{
    auto scene = makeTwoLevelScene();
    cv::Mat raster;
    // Level 1 is 50x50. This rect starts at (40,40) and is 20 wide, so half of it is outside.
    scene->readResampledLevelBlockChannels(1, cv::Rect(40, 40, 20, 20), cv::Size(20, 20), {}, raster);
    ASSERT_EQ(cv::Size(20, 20), raster.size());
    ASSERT_EQ(1u, scene->requests().size());
    // Only the valid 10x10 part was read, from scene coordinates (80,80).
    EXPECT_EQ(cv::Rect(80, 80, 20, 20), scene->requests()[0].rect);
    EXPECT_EQ(cv::Size(10, 10), scene->requests()[0].size);
    // DT_Byte background is 255. The overhanging columns and rows must all carry it.
    for (int y = 0; y < 20; ++y) {
        for (int x = 0; x < 20; ++x) {
            if (x < 10 && y < 10) continue;
            const cv::Vec3b pixel = raster.at<cv::Vec3b>(y, x);
            EXPECT_EQ(255, pixel[0]) << "x " << x << " y " << y;
            EXPECT_EQ(255, pixel[1]) << "x " << x << " y " << y;
            EXPECT_EQ(255, pixel[2]) << "x " << x << " y " << y;
        }
    }
}

// A rect entirely outside the level is not an error and must not reach the driver at all.
TEST(LevelReadingTests, defaultImplementationReturnsBackgroundForARectOutsideTheLevel)
{
    auto scene = makeTwoLevelScene();
    cv::Mat raster;
    scene->readResampledLevelBlockChannels(1, cv::Rect(200, 200, 20, 20), cv::Size(20, 20), {}, raster);
    ASSERT_EQ(cv::Size(20, 20), raster.size());
    EXPECT_TRUE(scene->requests().empty());
    cv::Mat background(20, 20, CV_8UC3, cv::Scalar(255, 255, 255));
    cv::Mat diff;
    cv::absdiff(raster, background, diff);
    EXPECT_EQ(0, cv::countNonZero(diff.reshape(1)));
}

// Resampling stays inside the named level: the request the driver receives keeps the
// level-derived scene rect and only the output size shrinks.
TEST(LevelReadingTests, defaultImplementationResamplesWithinTheNamedLevel)
{
    auto scene = makeTwoLevelScene();
    cv::Mat raster;
    scene->readResampledLevelBlockChannels(1, cv::Rect(0, 0, 40, 40), cv::Size(10, 10), {}, raster);
    ASSERT_EQ(1u, scene->requests().size());
    EXPECT_EQ(cv::Rect(0, 0, 80, 80), scene->requests()[0].rect);
    EXPECT_EQ(cv::Size(10, 10), scene->requests()[0].size);
    EXPECT_EQ(cv::Size(10, 10), raster.size());
}

// The 4D entry point assembles one plane per slice, each of them a level read.
TEST(LevelReadingTests, level4DBlockReadsOnePlanePerSlice)
{
    auto scene = makeTwoLevelScene();
    scene->setNumZSlices(3);
    cv::Mat raster;
    scene->readResampledLevel4DBlockChannels(1, cv::Rect(0, 0, 20, 20), cv::Size(20, 20), {},
                                             cv::Range(0, 3), cv::Range(0, 1), raster);
    ASSERT_EQ(3u, scene->requests().size());
    for (int slice = 0; slice < 3; ++slice) {
        EXPECT_EQ(cv::Rect(0, 0, 40, 40), scene->requests()[slice].rect) << "slice " << slice;
        EXPECT_EQ(slice, scene->requests()[slice].zSlice) << "slice " << slice;
    }
    ASSERT_EQ(3, raster.dims);
    EXPECT_EQ(20, raster.size[0]);
    EXPECT_EQ(20, raster.size[1]);
    EXPECT_EQ(3, raster.size[2]);
}

// The public Scene api reaches the same read as the CVScene one, through a caller supplied
// buffer. The gdal png is a one level scene, so a level 0 read of a rect must equal readBlock.
TEST(LevelReadingTests, publicSceneApiReadsALevelIntoABuffer)
{
    const std::string path = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, "GDAL");
    ASSERT_TRUE(slide != nullptr);
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);
    ASSERT_EQ(1, scene->getNumZoomLevels());

    const std::tuple<int, int, int, int> rect(100, 200, 300, 400);
    const std::tuple<int, int> size(300, 400);
    const int bufferSize = scene->getBlockSize(size, 0, 3, 1, 1);

    std::vector<uint8_t> viaBlock(bufferSize), viaLevel(bufferSize);
    scene->readBlock(rect, viaBlock.data(), viaBlock.size());
    scene->readResampledLevelBlockChannels(0, rect, size, {}, viaLevel.data(), viaLevel.size());
    EXPECT_EQ(viaBlock, viaLevel);
}

TEST(LevelReadingTests, publicSceneApiRejectsATooSmallBuffer)
{
    const std::string path = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, "GDAL");
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);

    std::vector<uint8_t> tooSmall(16);
    EXPECT_THROW(scene->readResampledLevelBlockChannels(
                     0, {0, 0, 300, 400}, {300, 400}, {}, tooSmall.data(), tooSmall.size()),
                 slideio::RuntimeError);
}

TEST(LevelReadingTests, publicSceneApiRejectsAnOutOfRangeLevel)
{
    const std::string path = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, "GDAL");
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);

    const std::tuple<int, int> size(64, 64);
    std::vector<uint8_t> buffer(scene->getBlockSize(size, 0, 3, 1, 1));
    EXPECT_THROW(scene->readResampledLevelBlockChannels(
                     5, {0, 0, 64, 64}, size, {}, buffer.data(), buffer.size()),
                 slideio::RuntimeError);
}
