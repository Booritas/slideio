// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// Pyramidal tiffs through the GDAL driver. Every fixture is written at run time
// with raw libtiff calls (gdaltiffwriter.cpp), and every level written is the
// exact raster the test compares against, so a level read must match bit for bit.
#include "slideio/drivers/gdal/gdalimagedriver.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/core/levelinfo.hpp"
#include "slideio/core/tools/tempfile.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/imagetools/libtiff.hpp"
#include "slideio/slideio/slideio.hpp"
#include "tests/main/gdaltiffwriter.hpp"

#include <gtest/gtest.h>
#include <opencv2/imgproc.hpp>
#include <functional>

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

    // Writes a file through a callback that receives the open handle.
    void writeTiff(const std::string& path, const std::function<void(libtiff::TIFF*)>& body)
    {
        libtiff::TIFF* tiff = libtiff::TIFFOpen(path.c_str(), "w");
        ASSERT_TRUE(tiff != nullptr);
        body(tiff);
        libtiff::TIFFClose(tiff);
    }

    // The base as a tiled directory carrying every other level as a tiled SubIFD.
    void writeSubIfdPyramid(const std::string& path, const std::vector<cv::Mat>& levels, int tileSize,
                            bool planar = false, uint8_t padValue = 255)
    {
        writeTiff(path, [&](libtiff::TIFF* tiff) {
            const int numSubIfds = static_cast<int>(levels.size()) - 1;
            writeTiledDirectory(tiff, levels[0], tileSize, planar, padValue, 0, numSubIfds);
            for (int level = 1; level < static_cast<int>(levels.size()); ++level) {
                writeTiledDirectory(tiff, levels[level], tileSize, planar, padValue, 1);
            }
        });
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
}

// The SubIFD layout libvips and bfconvert write: one scene, three levels.
TEST(GDALTiffPyramid, subIfdPyramidOpensAsOneScene)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    const auto levels = makePyramid(makeRgbImage(1000, 800), kSubIfdLevels);
    writeSubIfdPyramid(path, levels, 256);

    auto slide = openGdal(path);
    ASSERT_EQ(1, slide->getNumScenes());
    auto scene = slide->getScene(0);
    EXPECT_EQ(cv::Rect(0, 0, 1000, 800), scene->getRect());
    EXPECT_EQ(3, scene->getNumChannels());
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
        writeTiledDirectory(tiff, levels[0], 256, false, 255, 0);
        cv::Mat mask(800, 1000, CV_8UC1, cv::Scalar(255));
        writeTiledDirectory(tiff, mask, 256, false, 0, 4);
        writeTiledDirectory(tiff, levels[1], 256, false, 255, 1);
        writeTiledDirectory(tiff, levels[2], 256, false, 255, 1);
    });

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
        writeTiledDirectory(tiff, levels[0], 128, false, 255, 0);
        writeTiledDirectory(tiff, levels[1], 128, false, 255, 1);
    });

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
    writeSubIfdPyramid(path, levels, 256, true);

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
    writeSubIfdPyramid(path, levels, 64, false, 255);

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
    EXPECT_LE(cv::norm(expected, block, cv::NORM_INF), 1.);
    // No pixel is the pad pattern.
    cv::Mat isPad;
    cv::inRange(block, cv::Scalar(255, 255, 255), cv::Scalar(255, 255, 255), isPad);
    EXPECT_EQ(0, cv::countNonZero(isPad));
}

// A downscaled read of the whole scene into the size of level 2 is level 2 exactly,
// which only reading level 2 itself can produce.
TEST(GDALTiffPyramid, scaledReadPicksReducedLevel)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    const auto levels = makePyramid(makeRgbImage(1000, 800), kSubIfdLevels);
    writeSubIfdPyramid(path, levels, 256);

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
        writeStripedDirectory(tiff, levels[0], 0, 1);
        writeTiledDirectory(tiff, levels[1], 64, false, 255, 1);
    });

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
        writeTiledDirectory(tiff, page0, 64, false, 255);
        writeTiledDirectory(tiff, page1, 64, false, 255);
    });

    auto slide = openGdal(path);
    ASSERT_EQ(2, slide->getNumScenes());
    expectLevelsRead(slide->getScene(0), {page0});
    expectLevelsRead(slide->getScene(1), {page1});
}

// The scene holds no handle of the slide's: it stays readable, at every level,
// once the slide is gone.
TEST(GDALTiffPyramid, pyramidSceneOutlivesSlide)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    const auto levels = makePyramid(makeRgbImage(1000, 800), kSubIfdLevels);
    writeSubIfdPyramid(path, levels, 256);

    auto slide = openGdal(path);
    auto scene = slide->getScene(0);
    slide.reset();
    expectLevelsRead(scene, levels);
}

// No vendor driver claims a generic pyramidal tiff, so auto-detection lands in GDAL.
TEST(GDALTiffPyramid, autoDetectionLandsInGdal)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    const auto levels = makePyramid(makeRgbImage(1000, 800), kSubIfdLevels);
    writeSubIfdPyramid(path, levels, 256);

    auto slide = slideio::openSlide(path, "");
    ASSERT_TRUE(slide != nullptr);
    EXPECT_EQ("GDAL", slide->getDriverId());
    ASSERT_EQ(1, slide->getNumScenes());
    EXPECT_EQ(3, slide->getScene(0)->getNumZoomLevels());
}
