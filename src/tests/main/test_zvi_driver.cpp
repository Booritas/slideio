// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
//#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>


#include "slideio/core/tools/tools.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"
#include "tests/testlib/testtools.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include "slideio/drivers/zvi/zviimagedriver.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/slideio/slideio.hpp"
#include "slideio/core/metadata.hpp"

using namespace slideio;

TEST(ZVIImageDriver, DriverManager_getDriverIDs)
{
    std::vector<std::string> driverIds = slideio::ImageDriverManager::getDriverIDs();
    auto it = std::find(driverIds.begin(),driverIds.end(), "ZVI");
    EXPECT_FALSE(it==driverIds.end());
}

TEST(ZVIImageDriver, getID)
{
    slideio::ZVIImageDriver driver;
    const std::string id = driver.getID();
    EXPECT_EQ(id,"ZVI");
}

TEST(ZVIImageDriver, canOpenFile)
{
    slideio::ZVIImageDriver driver;
    EXPECT_TRUE(driver.canOpenFile("c:\\abbb\\a.zvi"));
    EXPECT_FALSE(driver.canOpenFile("c:\\abbb\\a.zvi.tmp"));
}

TEST(ZVIImageDriver, openSlide2D)
{
    slideio::ZVIImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Merged.zvi");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide.get() != nullptr);
    EXPECT_EQ(slide->getMetadataFormat(), slideio::MetadataFormat::JSON);
    EXPECT_FALSE(slide->getRawMetadata().empty());
    const slideio::Metadata& meta = slide->getMetadata();

    // File-level tags remain at the root.
    EXPECT_EQ(meta["Image Width (Pixel)"].asInt(),  1480);
    EXPECT_EQ(meta["Image Height (Pixel)"].asInt(), 1132);

    // Per-item tags appear under Channels[].
    const slideio::Metadata& channels = meta["Channels"];
    ASSERT_TRUE(channels.isArray());
    ASSERT_EQ(channels.size(), 3u);

    EXPECT_EQ(channels[0]["Channel Name"].asString(), std::string("Hoechst 33342"));
    EXPECT_EQ(channels[1]["Channel Name"].asString(), std::string("Cy3"));
    EXPECT_EQ(channels[2]["Channel Name"].asString(), std::string("FITC"));

    // 2D image: no ZSlices nesting.
    EXPECT_FALSE(channels[0].contains("ZSlices"));

    const int sceneCount = slide->getNumScenes();
    ASSERT_EQ(sceneCount, 1);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene.get() != nullptr);
    const auto rect = scene->getRect();
    EXPECT_EQ(rect.x, 0);
    EXPECT_EQ(rect.y, 0);
    EXPECT_EQ(rect.width, 1480);
    EXPECT_EQ(rect.height, 1132);
    EXPECT_EQ(scene->getNumChannels(), 3);
    EXPECT_EQ(scene->getNumZSlices(), 1);
    EXPECT_EQ(scene->getNumTFrames(), 1);
    EXPECT_EQ(scene->getChannelDataType(0), slideio::DataType::DT_Int16);
    EXPECT_EQ(scene->getChannelDataType(1), slideio::DataType::DT_Int16);
    EXPECT_EQ(scene->getChannelDataType(2), slideio::DataType::DT_Int16);
    EXPECT_EQ(scene->getChannelName(0), std::string("Hoechst 33342"));
    EXPECT_EQ(scene->getChannelName(1), std::string("Cy3"));
    EXPECT_EQ(scene->getChannelName(2), std::string("FITC"));
    EXPECT_EQ(scene->getName(), std::string("RQ26033_04310292C0004S_Calu3_amplified_100x_21Jun2012 ic zsm.zvi"));
    EXPECT_EQ(scene->getCompression(), Compression::Uncompressed);
	EXPECT_EQ(scene->getMetadataFormat(), slideio::MetadataFormat::None);

    auto res = scene->getResolution();
    EXPECT_DOUBLE_EQ(res.x, 0.0645e-6);
    EXPECT_DOUBLE_EQ(res.y, 0.0645e-6);
}

TEST(ZVIImageDriver, openSlide3D)
{
    slideio::ZVIImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Stacked.zvi");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide.get() != nullptr);
    const int sceneCount = slide->getNumScenes();
    ASSERT_EQ(sceneCount, 1);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene.get() != nullptr);
    const auto rect = scene->getRect();
    EXPECT_EQ(rect.x, 0);
    EXPECT_EQ(rect.y, 0);
    EXPECT_EQ(rect.width, 1388);
    EXPECT_EQ(rect.height, 1040);
    EXPECT_EQ(scene->getNumChannels(), 3);
    EXPECT_EQ(scene->getNumZSlices(), 13);
    EXPECT_EQ(scene->getNumTFrames(), 1);
    EXPECT_EQ(scene->getChannelDataType(0), slideio::DataType::DT_Int16);
    EXPECT_EQ(scene->getChannelDataType(1), slideio::DataType::DT_Int16);
    EXPECT_EQ(scene->getChannelDataType(2), slideio::DataType::DT_Int16);
    EXPECT_EQ(scene->getChannelName(0), std::string("Hoechst 33342"));
    EXPECT_EQ(scene->getChannelName(1), std::string("Cy3"));
    EXPECT_EQ(scene->getChannelName(2), std::string("FITC"));
    auto res = scene->getResolution();
    EXPECT_DOUBLE_EQ(res.x, 0.0645e-6);
    EXPECT_DOUBLE_EQ(res.y, 0.0645e-6);
    auto zres = scene->getZSliceResolution();
    EXPECT_DOUBLE_EQ(zres, 0.25e-6);

    EXPECT_EQ(slide->getMetadataFormat(), slideio::MetadataFormat::JSON);
    const slideio::Metadata& meta = slide->getMetadata();
    const slideio::Metadata& channels = meta["Channels"];
    ASSERT_TRUE(channels.isArray());
    // Zeiss-1-Stacked.zvi has one channel and multiple z-slices.
    ASSERT_GE(channels.size(), 1u);

    const slideio::Metadata& ch0 = channels[0];
    ASSERT_TRUE(ch0.contains("ZSlices"));
    const slideio::Metadata& zSlices = ch0["ZSlices"];
    ASSERT_TRUE(zSlices.isArray());
    EXPECT_GT(zSlices.size(), 1u);

    // Channel Name (if present) is hoisted to the channel object, not duplicated per ZSlice.
    if (ch0.contains("Channel Name")) {
        EXPECT_FALSE(zSlices[0].contains("Channel Name"));
    }
}

TEST(ZVIImageDriver, openSlideMosaic)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full image dataset is not available";
    }

    slideio::ZVIImageDriver driver;
    std::string filePath = TestTools::getFullTestImagePath("zvi", "openslide/Zeiss-3-Mosaic.zvi");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide.get() != nullptr);
    const int sceneCount = slide->getNumScenes();
    ASSERT_EQ(sceneCount, 1);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene.get() != nullptr);
    const auto rect = scene->getRect();
    EXPECT_EQ(rect.x, 0);
    EXPECT_EQ(rect.y, 0);
    EXPECT_EQ(rect.width, 13882);
    EXPECT_EQ(rect.height, 21631);
    EXPECT_EQ(scene->getNumChannels(), 3);
    EXPECT_EQ(scene->getNumZSlices(), 1);
    EXPECT_EQ(scene->getNumTFrames(), 1);
    EXPECT_EQ(scene->getChannelDataType(0), slideio::DataType::DT_Int16);
    EXPECT_EQ(scene->getChannelDataType(1), slideio::DataType::DT_Int16);
    EXPECT_EQ(scene->getChannelDataType(2), slideio::DataType::DT_Int16);
    EXPECT_EQ(scene->getChannelName(0), std::string("Hoechst 33342"));
    EXPECT_EQ(scene->getChannelName(1), std::string("Alexa 488"));
    EXPECT_EQ(scene->getChannelName(2), std::string("Cy3"));
    auto res = scene->getResolution();
    EXPECT_DOUBLE_EQ(res.x, 0.3225e-6);
    EXPECT_DOUBLE_EQ(res.y, 0.3225e-6);
    auto zres = scene->getZSliceResolution();
    EXPECT_DOUBLE_EQ(zres, 1);
}

TEST(ZVIImageDriver, readBlock3Layers)
{
    slideio::ZVIImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Merged.zvi");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide.get() != nullptr);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene.get() != nullptr);
    const auto rect = scene->getRect();
    cv::Mat raster;
    std::vector<int> channels;
    scene->readBlockChannels(rect, channels, raster);
    EXPECT_EQ(raster.cols, rect.width);
    EXPECT_EQ(raster.rows, rect.height);

    for (int channel = 0; channel < 3; channel++) {
        cv::Mat channelRaster, channelRasterTest;
        cv::extractChannel(raster, channelRaster, channel);
        std::string channelName = std::string("Zeiss-1-Merged-ch") + std::to_string(channel) + ".tif";
        std::string channelPath = TestTools::getTestImagePath("zvi", channelName);
        slideio::ImageTools::readSmallImageRaster(channelPath, channelRasterTest);
		double score = ImageTools::computeSimilarity2(channelRaster, channelRasterTest);
		EXPECT_GT(score, 0.999);
    }
}

TEST(ZVIImageDriver, readBlockROI)
{
    slideio::ZVIImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Merged.zvi");

    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide.get() != nullptr);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene.get() != nullptr);
    const auto rect = scene->getRect();
    cv::Mat raster;
    std::vector<int> channels = { 0 };
    scene->readBlockChannels(rect, channels, raster);
    EXPECT_EQ(raster.cols, rect.width);
    EXPECT_EQ(raster.rows, rect.height);

    cv::Mat channelRaster;
    std::string channelPath = TestTools::getTestImagePath("zvi", "Zeiss-1-Merged-ch0.tif");
    slideio::ImageTools::readSmallImageRaster(channelPath, channelRaster);
    cv::Mat channelDiff = cv::abs(raster - channelRaster);
    double min(0), max(0);
    cv::minMaxLoc(channelDiff, &min, &max);
    EXPECT_EQ(min, 0);
    EXPECT_EQ(max, 0);

    const int width4 = rect.width / 4;
    const int height4 = rect.height / 4;
    cv::Rect rectRoi(width4, height4, width4, height4);
    scene->readBlockChannels(rectRoi, channels, raster);
    EXPECT_EQ(raster.cols, width4);
    EXPECT_EQ(raster.rows, height4);

    cv::Mat channelRoi = channelRaster(rectRoi);
    channelDiff = cv::abs(raster - channelRoi);
    min = 0; max = 0;
    cv::minMaxLoc(channelDiff, &min, &max);
    EXPECT_EQ(min, 0);
    EXPECT_EQ(max, 0);

    const int width2 = rect.width / 2;
    const int height2 = rect.height / 2;
    cv::Rect rectRoi2(width4, height4, width2, height2);
    scene->readResampledBlockChannels(rectRoi, { width4, height4 }, channels, raster);
    EXPECT_EQ(raster.cols, width4);
    EXPECT_EQ(raster.rows, height4);
    channelRoi = channelRaster(rectRoi);
    cv::Mat channelRoiResized;
    cv::resize(channelRoi, channelRoiResized, { width4, height4 });

    double similarity = ImageTools::computeSimilarity(raster, channelRoiResized);
    EXPECT_DOUBLE_EQ(1., similarity);
}

TEST(ZVIImageDriver, readBlock3DSlice)
{
    slideio::ZVIImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Stacked.zvi");

    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide.get() != nullptr);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene.get() != nullptr);
    const auto rect = scene->getRect();
    cv::Mat raster;
    std::vector<int> channels = { 1 };
    const int zSlices = scene->getNumZSlices();

    cv::Rect rectRoi = rect;
    cv::Size sizeRoi(rect.width, rect.height);
    cv::Range zSliceRange(6, 7);
    cv::Range tFrameRange(0, 1);
    scene->readResampled4DBlockChannels(rectRoi, sizeRoi, channels, zSliceRange, tFrameRange, raster);
    EXPECT_EQ(raster.dims, 2);
    EXPECT_EQ(raster.channels(), 1);
    EXPECT_EQ(raster.cols, sizeRoi.width);
    EXPECT_EQ(raster.rows, sizeRoi.height);
    cv::Mat rawSlice(rect.height, rect.width, CV_16SC1);
    std::string slicePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Stacked/zvi_slice_6_channel_1");
    TestTools::readRawImage(slicePath, rawSlice);

    double similarity = ImageTools::computeSimilarity(raster, rawSlice);
    EXPECT_DOUBLE_EQ(1., similarity);
}

TEST(ZVIImageDriver, readBlock3DROI)
{
    slideio::ZVIImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Stacked.zvi");

    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide.get() != nullptr);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene.get() != nullptr);
    const auto rect = scene->getRect();
    cv::Mat raster;
    std::vector<int> channels = { 1 };
    const int zSlices = scene->getNumZSlices();

    const int width2 = rect.width / 2;
    const int height2 = rect.height / 2;
    const int width4 = rect.width / 4;
    const int height4 = rect.height / 4;
    const cv::Rect rectRoi(width4, height4, width2, height2);
    const cv::Size sizeRoi(width2, height2);
    const cv::Range zSliceRange(6, 7);
    const cv::Range tFrameRange(0, 1);

    scene->readResampled4DBlockChannels(rectRoi, sizeRoi, channels, zSliceRange, tFrameRange,  raster);
    EXPECT_EQ(raster.dims, 2);
    EXPECT_EQ(raster.channels(), 1);
    EXPECT_EQ(raster.cols, sizeRoi.width);
    EXPECT_EQ(raster.rows, sizeRoi.height);
    cv::Mat rawSlice(rect.height, rect.width, CV_16SC1);
    std::string slicePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Stacked/zvi_slice_6_channel_1");
    TestTools::readRawImage(slicePath, rawSlice);

    cv::Mat roi = rawSlice(rectRoi);

    double similarity = ImageTools::computeSimilarity(raster, roi);
    EXPECT_DOUBLE_EQ(1., similarity);
}

TEST(ZVIImageDriver, readBlock3DROIResized)
{
    slideio::ZVIImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Stacked.zvi");

    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide.get() != nullptr);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene.get() != nullptr);
    const auto rect = scene->getRect();
    cv::Mat raster;
    std::vector<int> channels = { 1 };
    const int zSlices = scene->getNumZSlices();

    const int width2 = rect.width / 2;
    const int height2 = rect.height / 2;
    const int width4 = rect.width / 4;
    const int height4 = rect.height / 4;
    const cv::Rect rectRoi(width4, height4, width2, height2);
    const cv::Size sizeRoi(width4, height4);
    const cv::Range zSliceRange(6, 7);
    const cv::Range tFrameRange(0, 1);

    scene->readResampled4DBlockChannels(rectRoi, sizeRoi, channels, zSliceRange, tFrameRange, raster);
    EXPECT_EQ(raster.dims, 2);
    EXPECT_EQ(raster.channels(), 1);
    EXPECT_EQ(raster.cols, sizeRoi.width);
    EXPECT_EQ(raster.rows, sizeRoi.height);
    cv::Mat rawSlice(rect.height, rect.width, CV_16SC1);
    std::string slicePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Stacked/zvi_slice_6_channel_1");
    TestTools::readRawImage(slicePath, rawSlice);
    cv::Mat rawRoi = rawSlice(rectRoi);
    cv::Mat rawRoiResized;
    cv::resize(rawRoi, rawRoiResized, sizeRoi, 0, 0, cv::INTER_NEAREST);

    double similarity = ImageTools::computeSimilarity(raster, rawRoiResized);
    EXPECT_LT(0.95, similarity);
}


TEST(ZVIImageDriver, readBlock3DROIResizedMultiSlice)
{
    slideio::ZVIImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Stacked.zvi");

    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide.get() != nullptr);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene.get() != nullptr);
    const auto sceneRect = scene->getRect();
    cv::Mat raster;
    std::vector<int> channels = { 1, 2 };
    const int zSlices = scene->getNumZSlices();

    const int width2 = sceneRect.width / 2;
    const int height2 = sceneRect.height / 2;
    const int width4 = sceneRect.width / 4;
    const int height4 = sceneRect.height / 4;
    const cv::Rect rectRoi(width4, height4, width2, height2);
    const cv::Size sizeRoi(width4, height4);
    const cv::Range zSliceRange(6, 9);
    const cv::Range tFrameRange(0, 1);

    scene->readResampled4DBlockChannels(rectRoi, sizeRoi, channels, zSliceRange, tFrameRange, raster);

    EXPECT_EQ(raster.dims, 3);
    EXPECT_EQ(raster.channels(), 2);
    EXPECT_EQ(raster.size[0], sizeRoi.height);
    EXPECT_EQ(raster.size[1], sizeRoi.width);
    EXPECT_EQ(raster.size[2], 3);

    cv::Mat sliceRaster;
    CVTools::extractSliceFrom3D(raster, 1, sliceRaster);

    EXPECT_EQ(sliceRaster.dims, 2);
    EXPECT_EQ(sliceRaster.channels(), 2);
    EXPECT_EQ(sliceRaster.rows, sizeRoi.height);
    EXPECT_EQ(sliceRaster.cols, sizeRoi.width);

    cv::Mat channelRaster;
    cv::extractChannel(sliceRaster, channelRaster, 1);

    std::string slicePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Stacked/zvi_slice_7_channel_2");

    cv::Mat rawSlice(sceneRect.height, sceneRect.width, CV_16SC1);
    TestTools::readRawImage(slicePath, rawSlice);
    cv::Mat rawRoi = rawSlice(rectRoi);
    cv::Mat resizedRoi;
    cv::resize(rawRoi, resizedRoi, sizeRoi, 0, 0, cv::INTER_NEAREST);

    double similarity = ImageTools::computeSimilarity(channelRaster, resizedRoi);
    EXPECT_LT(0.95, similarity);
}

TEST(ZVIImageDriver, readBlock)
{
    if (!TestTools::isFullTestEnabled()) {
        GTEST_SKIP() << "Skip full test because full dataset is not enabled";
    }
    slideio::ZVIImageDriver driver;
    std::string filePath = TestTools::getFullTestImagePath("zvi", "mouse/20140207_mouse_2cell_H2AUb_HA_DAPI_inj_002.zvi");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide.get() != nullptr);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene.get() != nullptr);
    const auto rect = scene->getRect();
    auto dt = scene->getChannelDataType(0);
    auto channels = scene->getNumChannels();
    const double asp = (double)rect.height / (double)rect.width;
    cv::Mat raster;
    std::vector<int> channelIndices = { 0, 1, 2 };
    scene->readBlockChannels(rect, channelIndices, raster);
    EXPECT_EQ(raster.cols, rect.width);
    EXPECT_EQ(raster.rows, rect.height);
    EXPECT_EQ(channels, 3);
    EXPECT_EQ(dt, slideio::DataType::DT_Int16);
}

TEST(ZVIImageDriver, readBlockTOMM)
{
    slideio::ZVIImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("zvi", "TOMMAlexaFluor647.zvi");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide.get() != nullptr);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene.get() != nullptr);
    const auto rect = scene->getRect();
    EXPECT_EQ(rect.width, 1388);
    EXPECT_EQ(rect.height, 1040);
    const auto dt = scene->getChannelDataType(0);
    const auto channels = scene->getNumChannels();
    EXPECT_EQ(dt, DataType::DT_Int16);
    EXPECT_EQ(channels, 1);
}

TEST(ZVIImageDriver, readBlock3D)
{
    if (!TestTools::isFullTestEnabled()) {
        GTEST_SKIP() << "Skip full test because full dataset is not enabled";
    }
    slideio::ZVIImageDriver driver;
    std::string filePath = TestTools::getFullTestImagePath("zvi", "mouse/20140505_mouse_2cell_H2AUb_RING1B_DAPI_T_005.zvi");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide.get() != nullptr);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene.get() != nullptr);
    const auto rect = scene->getRect();
    auto dt = scene->getChannelDataType(0);
    auto channels = scene->getNumChannels();
    const double asp = (double)rect.height / (double)rect.width;
    cv::Mat raster;
    std::vector<int> channelIndices = { 0 };
    cv::Size size = { rect.width, rect.height };
    cv::Range slices = { 0, 10 };
    cv::Range frames = { 0, 1 };
    scene->readResampled4DBlockChannels(rect, size, channelIndices, slices, frames, raster);
    EXPECT_EQ(raster.size[0], size.height);
    EXPECT_EQ(raster.size[1], size.width);
    EXPECT_EQ(raster.size[2], 10);
    EXPECT_EQ(dt, slideio::DataType::DT_Int16);
}

TEST(ZVIImageDriver, readBlock3D_emptyChannelIndices)
{
    slideio::ZVIImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Stacked.zvi");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide.get() != nullptr);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene.get() != nullptr);
    const auto rect = scene->getRect();
    auto dt = scene->getChannelDataType(0);
    auto channels = scene->getNumChannels();
    const double asp = (double)rect.height / (double)rect.width;
    cv::Mat raster;
    std::vector<int> channelIndices;
    cv::Size size = { rect.width, rect.height };
    cv::Range slices = { 0, 10 };
    cv::Range frames = { 0, 1 };
    scene->readResampled4DBlockChannels(rect, size, channelIndices, slices, frames, raster);
    EXPECT_EQ(raster.size[0], size.height);
    EXPECT_EQ(raster.size[1], size.width);
    EXPECT_EQ(raster.size[2], 10);
    EXPECT_EQ(dt, slideio::DataType::DT_Int16);
}

TEST(ZVIImageDriver, openFileUtf8)
{
    {
        std::string filePath = TestTools::getFullTestImagePath("unicode", u8"тест/TOMMAlexaFluor647.zvi");
        slideio::ZVIImageDriver driver;
        std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
        int dirCount = slide->getNumScenes();
        ASSERT_EQ(dirCount, 1);
        std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
        auto rect = scene->getRect();
        cv::Rect expectedRect(0, 0, 1388, 1040);
        EXPECT_EQ(rect, expectedRect);
        cv::Mat raster;
        cv::Size size;
        double scale = 0.5;
        size.width = std::lround(double(rect.width) * scale);
        size.height = std::lround(double(rect.height) * scale);
        rect.x = rect.y = 0;
        scene->readResampledBlock(rect, size, raster);
        EXPECT_EQ(raster.cols, size.width);
        EXPECT_EQ(raster.rows, size.height);
    }
}

TEST(ZVIImageDriver, zoomLevel)
{
    slideio::ZVIImageDriver driver;
    const std::string filePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Stacked.zvi");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide.get() != nullptr);
    auto scene = slide->getScene(0);
    EXPECT_EQ(1, scene->getNumZoomLevels());
    const LevelInfo* zoomLevel = scene->getZoomLevelInfo(0);
    ASSERT_TRUE(zoomLevel != nullptr);
    EXPECT_EQ(zoomLevel->getMagnification(), scene->getMagnification());
    EXPECT_EQ(zoomLevel->getScale(), 1.0);
    EXPECT_EQ(zoomLevel->getSize(), Tools::cvSizeToSize(scene->getRect().size()));
    EXPECT_EQ(zoomLevel->getTileSize(), Size(1388, 1040));
}

TEST(ZVIImageDriver, multiThreadSceneAccess) {
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() <<
            "Skip the test because full dataset is not enabled";
    }
    std::string filePath = TestTools::getFullTestImagePath("zvi", "mouse/20140505_mouse_2cell_H2AUb_RING1B_DAPI_T_005.zvi");
    slideio::ZVIImageDriver driver;
    TestTools::multiThreadedTest(filePath, driver);
}

TEST(ZVIImageDriver, getSceneIndex)
{
    if (!TestTools::isFullTestEnabled()) {
        GTEST_SKIP() <<
            "Skip the test because full dataset is not enabled";
    }
    const std::string filePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Stacked.zvi");
    auto slide = slideio::openSlide(filePath, "AUTO");
    ASSERT_TRUE(slide);
    EXPECT_EQ("ZVI", slide->getDriverId());
    const int numScenes = slide->getNumScenes();
    EXPECT_EQ(1, numScenes);
    for (int iScene = 0; iScene < numScenes; ++iScene) {
        std::shared_ptr<slideio::CVScene> scene = slide->getScene(iScene)->getCVScene();
        EXPECT_TRUE(scene.get() != nullptr);
        EXPECT_EQ(iScene, scene->getSceneIndex());
        EXPECT_EQ(filePath, scene->getFilePath());
		EXPECT_EQ("ZVI", scene->getDriverId());
    }
    const int numImages = slide->getNumAuxImages();
    ASSERT_EQ(numImages, 0);
    std::list<std::string> imageNames = slide->getAuxImageNames();
    for (auto& name : imageNames) {
        auto scene = slide->getAuxImage("label")->getCVScene();
        EXPECT_TRUE(scene.get() != nullptr);
        EXPECT_EQ(-1, scene->getSceneIndex());
        EXPECT_EQ(filePath, scene->getFilePath());
    }
}

namespace
{
    // Minimal compound-file directory reader: enough to find where a named
    // directory entry sits in the file so that a test can patch it.
    struct CfbDirectory
    {
        std::vector<uint8_t> bytes;      // the whole file
        std::vector<size_t> entryOffset; // file offset of each 128 byte entry
        std::vector<uint32_t> fat;
        std::vector<uint32_t> minifat;
        size_t sectorSize = 0;
        size_t miniSectorSize = 0;
        size_t miniCutoff = 0;
        uint32_t rootStart = 0;

        static uint32_t u32(const std::vector<uint8_t>& b, size_t off) {
            return static_cast<uint32_t>(b[off]) | (static_cast<uint32_t>(b[off + 1]) << 8)
                 | (static_cast<uint32_t>(b[off + 2]) << 16) | (static_cast<uint32_t>(b[off + 3]) << 24);
        }
        static uint16_t u16(const std::vector<uint8_t>& b, size_t off) {
            return static_cast<uint16_t>(b[off] | (b[off + 1] << 8));
        }

        std::string name(size_t entry) const {
            const size_t off = entryOffset[entry];
            size_t len = u16(bytes, off + 0x40);
            if (len > 64) len = 64;
            std::string result;
            for (size_t i = 0; i + 1 < len && bytes[off + i]; i += 2) {
                result.push_back(static_cast<char>(bytes[off + i]));
            }
            return result;
        }
        uint8_t type(size_t entry) const { return bytes[entryOffset[entry] + 0x42]; }
        uint32_t prev(size_t entry) const { return u32(bytes, entryOffset[entry] + 0x44); }
        uint32_t next(size_t entry) const { return u32(bytes, entryOffset[entry] + 0x48); }
        uint32_t child(size_t entry) const { return u32(bytes, entryOffset[entry] + 0x4C); }
        uint32_t start(size_t entry) const { return u32(bytes, entryOffset[entry] + 0x74); }
        uint32_t size(size_t entry) const { return u32(bytes, entryOffset[entry] + 0x78); }

        // The header occupies the whole first sector, whatever the sector size.
        size_t sectorAt(uint32_t sector) const {
            return (static_cast<size_t>(sector) + 1) * sectorSize;
        }
        static uint32_t follow(const std::vector<uint32_t>& table, uint32_t sector, size_t steps) {
            for (size_t i = 0; i < steps; ++i) {
                if (sector >= table.size()) return 0xFFFFFFFF;
                sector = table[sector];
            }
            return sector;
        }

        // File offset of byte `offset` of the stream held by `entry`. Streams
        // below the cutoff live in the mini stream, which is itself the root
        // entry's stream, so those take two hops.
        size_t byteOffset(size_t entry, size_t offset) const {
            if (size(entry) >= miniCutoff) {
                const uint32_t sector = follow(fat, start(entry), offset / sectorSize);
                if (sector > 0xFFFFFFFA) return 0;
                return sectorAt(sector) + offset % sectorSize;
            }
            const uint32_t mini = follow(minifat, start(entry), offset / miniSectorSize);
            if (mini > 0xFFFFFFFA) return 0;
            const size_t inMiniStream =
                static_cast<size_t>(mini) * miniSectorSize + offset % miniSectorSize;
            const uint32_t sector = follow(fat, rootStart, inMiniStream / sectorSize);
            if (sector > 0xFFFFFFFA) return 0;
            return sectorAt(sector) + inMiniStream % sectorSize;
        }

        std::vector<uint8_t> readStream(size_t entry, size_t count) const {
            const size_t want = std::min<size_t>(count, size(entry));
            std::vector<uint8_t> out(want);
            for (size_t i = 0; i < want; ++i) {
                const size_t off = byteOffset(entry, i);
                out[i] = off ? bytes[off] : 0;
            }
            return out;
        }
        void writeI32(size_t entry, size_t offset, int32_t value) {
            for (size_t i = 0; i < 4; ++i) {
                const size_t off = byteOffset(entry, offset + i);
                if (off) bytes[off] = static_cast<uint8_t>((value >> (8 * i)) & 0xFF);
            }
        }
    };

    // Steps over one typed item, mirroring ZVIUtils::skipItem. Returns the
    // offset just past it.
    size_t skipTypedItem(const std::vector<uint8_t>& b, size_t pos)
    {
        const uint16_t type = CfbDirectory::u16(b, pos);
        pos += 2;
        switch (type) {
        case 0x00: case 0x01: return pos;
        case 0x10: case 0x11: return pos + 1;
        case 0x02: case 0x12: case 0x0B: return pos + 2;
        case 0x03: case 0x13: case 0x16: case 0x17: case 0x04: case 0x0A: return pos + 4;
        case 0x14: case 0x15: case 0x05: case 0x07: case 0x06: return pos + 8;
        case 0x09: case 0x0D: case 0x0E: return pos + 16;
        case 0x08: case 0x41: case 0x45: case 0x3F: case 0x2000:
            return pos + 4 + CfbDirectory::u32(b, pos);
        case 0x42:
            return pos + 2 + CfbDirectory::u16(b, pos);
        default:
            return b.size(); // unknown: stop
        }
    }

    bool readCfbDirectory(const std::string& path, CfbDirectory& dir)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;
        dir.bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        if (dir.bytes.size() < 512) return false;

        dir.sectorSize = static_cast<size_t>(1) << CfbDirectory::u16(dir.bytes, 0x1E);
        dir.miniSectorSize = static_cast<size_t>(1) << CfbDirectory::u16(dir.bytes, 0x20);
        dir.miniCutoff = CfbDirectory::u32(dir.bytes, 0x38);
        const size_t sectorSize = dir.sectorSize;
        auto sectorAt = [&](uint32_t sector) { return dir.sectorAt(sector); };

        // DIFAT slots are filled in order, so the first invalid one ends the
        // table: skipping past it would shift every later FAT index.
        for (size_t i = 0; i < 109; ++i) {
            const uint32_t sector = CfbDirectory::u32(dir.bytes, 0x4C + i * 4);
            if (sector > 0xFFFFFFFA) break;
            const size_t off = sectorAt(sector);
            if (off + sectorSize > dir.bytes.size()) break;
            for (size_t j = 0; j < sectorSize / 4; ++j) {
                dir.fat.push_back(CfbDirectory::u32(dir.bytes, off + j * 4));
            }
        }
        // A file needing DIFAT extension sectors is far larger than any test file.
        uint32_t sector = CfbDirectory::u32(dir.bytes, 0x30);
        while (sector <= 0xFFFFFFFA && sector < dir.fat.size()) {
            const size_t off = sectorAt(sector);
            if (off + sectorSize > dir.bytes.size()) break;
            for (size_t j = 0; j < sectorSize / 128; ++j) {
                dir.entryOffset.push_back(off + j * 128);
            }
            sector = dir.fat[sector];
        }
        if (dir.entryOffset.empty()) return false;

        dir.rootStart = dir.start(0);
        uint32_t miniSector = CfbDirectory::u32(dir.bytes, 0x3C);
        while (miniSector <= 0xFFFFFFFA && miniSector < dir.fat.size()) {
            const size_t off = sectorAt(miniSector);
            if (off + sectorSize > dir.bytes.size()) break;
            for (size_t j = 0; j < sectorSize / 4; ++j) {
                dir.minifat.push_back(CfbDirectory::u32(dir.bytes, off + j * 4));
            }
            miniSector = dir.fat[miniSector];
        }
        return true;
    }

    void collectSiblings(const CfbDirectory& dir, uint32_t entry, std::vector<uint32_t>& result)
    {
        if (entry >= dir.entryOffset.size()) return;
        if (std::find(result.begin(), result.end(), entry) != result.end()) return;
        result.push_back(entry);
        collectSiblings(dir, dir.prev(entry), result);
        collectSiblings(dir, dir.next(entry), result);
    }

    // The entry reached by following `path` from the root storage, or
    // entryOffset.size() if there is none. Entry names are not unique across
    // the document -- a ZVI file has both /Image/Item(0) and
    // /Image/Layers/Item(0) -- so only the full path identifies an entry.
    size_t findEntryByPath(const CfbDirectory& dir, const std::string& path)
    {
        const size_t missing = dir.entryOffset.size();
        uint32_t current = 0; // the root entry
        size_t start = 1;     // skip the leading '/'
        while (start < path.size())
        {
            const size_t end = std::min(path.find('/', start), path.size());
            const std::string component = path.substr(start, end - start);
            std::vector<uint32_t> children;
            collectSiblings(dir, dir.child(current), children);
            const auto found = std::find_if(children.begin(), children.end(),
                [&](uint32_t child) { return dir.name(child) == component; });
            if (found == children.end()) return missing;
            current = *found;
            start = end + 1;
        }
        return current;
    }

    // Copies `src` to `dst`, renaming the <Contents> entry of /Image/Item(itemIndex)
    // so that the item storage is present but its raster stream is not -- the shape
    // of the file reported in github.com/Booritas/slideio/discussions/73.
    // Returns false if the entry could not be located.
    // Copies `src` to `dst` with the <Contents> stream at `path` renamed, so
    // the storage is present but the stream the driver looks for is not.
    bool copyZviWithContentsRenamed(const std::string& src, const std::string& dst,
                                    const std::string& path)
    {
        CfbDirectory dir;
        if (!readCfbDirectory(src, dir)) return false;

        const size_t entry = findEntryByPath(dir, path);
        if (entry == dir.entryOffset.size() || dir.type(entry) != 2) return false;

        // Same length, so the name length field stays valid: Contents -> Contentz.
        dir.bytes[dir.entryOffset[entry] + 14] = static_cast<uint8_t>('z');
        std::ofstream out(dst, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(reinterpret_cast<const char*>(dir.bytes.data()), dir.bytes.size());
        return out.good();
    }

    // The item's raster stream: the item itself becomes unreadable.
    bool copyZviWithoutItemContents(const std::string& src, const std::string& dst, int itemIndex)
    {
        return copyZviWithContentsRenamed(
            src, dst, "/Image/Item(" + std::to_string(itemIndex) + ")/Contents");
    }

    // The item's tag stream: the item stays readable but loses its channel
    // name and its tile position.
    bool copyZviWithoutItemTags(const std::string& src, const std::string& dst, int itemIndex)
    {
        return copyZviWithContentsRenamed(
            src, dst, "/Image/Item(" + std::to_string(itemIndex) + ")/Tags/Contents");
    }
}

// An item storage whose <Contents> stream is absent used to abort the whole
// open with "Invalid stream path: /Image/Item(n)/Contents". The item it
// describes is unreadable, but the other items are not.
TEST(ZVIImageDriver, openSlideWithMissingItemContents)
{
    const std::string filePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Merged.zvi");
    const std::string patchedPath =
        (std::filesystem::temp_directory_path() / "zvi-missing-item-contents.zvi").string();
    ASSERT_TRUE(copyZviWithoutItemContents(filePath, patchedPath, 1))
        << "could not build the test file from " << filePath;

    slideio::ZVIImageDriver driver;
    std::shared_ptr<slideio::CVSlide> reference = driver.openFile(filePath);
    ASSERT_TRUE(reference.get() != nullptr);
    const int referenceChannels = reference->getScene(0)->getNumChannels();
    ASSERT_EQ(referenceChannels, 3);
    reference.reset();

    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(patchedPath);
    ASSERT_TRUE(slide.get() != nullptr);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    ASSERT_TRUE(scene.get() != nullptr);
    // The two items that still have a <Contents> stream are the ones kept.
    EXPECT_EQ(scene->getNumChannels(), 2);
    EXPECT_EQ(scene->getRect().width, 1480);
    EXPECT_EQ(scene->getRect().height, 1132);

    cv::Mat raster;
    scene->readBlock(scene->getRect(), raster);
    EXPECT_EQ(raster.cols, 1480);
    EXPECT_EQ(raster.rows, 1132);
    EXPECT_EQ(raster.channels(), 2);

    // The scene holds the compound document open; both handles have to go
    // before the file can be removed on Windows.
    scene.reset();
    slide.reset();
    std::error_code ignored;
    std::filesystem::remove(patchedPath, ignored);
}

// A file with no readable item at all has to say so, not crash or hand back an
// empty scene.
TEST(ZVIImageDriver, openSlideWithoutAnyItemContents)
{
    const std::string filePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Merged.zvi");
    const std::string patchedPath =
        (std::filesystem::temp_directory_path() / "zvi-no-item-contents.zvi").string();
    ASSERT_TRUE(copyZviWithoutItemContents(filePath, patchedPath, 0));
    for (int item = 1; item < 3; ++item) {
        ASSERT_TRUE(copyZviWithoutItemContents(patchedPath, patchedPath, item));
    }

    slideio::ZVIImageDriver driver;
    EXPECT_THROW(driver.openFile(patchedPath), std::exception);
    std::error_code ignored;
    std::filesystem::remove(patchedPath, ignored);
}

// Removing one item leaves a hole in a (channel, slice) pair while both the
// channel and the slice are still present elsewhere in the document, so the
// scene keeps advertising them. Reading that pair used to abort the block with
// "Cannot find image item for channel 1 and slice 5"; the channel that is not
// stored is filled instead, and the channels that are stored still read.
TEST(ZVIImageDriver, readBlockWithMissingChannelSlicePair)
{
    const std::string filePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Stacked.zvi");
    const std::string patchedPath =
        (std::filesystem::temp_directory_path() / "zvi-missing-channel-slice.zvi").string();
    // /Image/Item(18) is channel 1 of z-slice 5 of that file.
    const int missingChannel = 1;
    const int missingSlice = 5;
    ASSERT_TRUE(copyZviWithoutItemContents(filePath, patchedPath, 18))
        << "could not build the test file from " << filePath;

    slideio::ZVIImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(patchedPath);
    ASSERT_TRUE(slide.get() != nullptr);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    ASSERT_TRUE(scene.get() != nullptr);
    // Every channel and every slice is still carried by some other item.
    EXPECT_EQ(scene->getNumChannels(), 3);
    EXPECT_EQ(scene->getNumZSlices(), 13);

    const cv::Rect block(0, 0, 400, 300);
    cv::Mat raster;
    ASSERT_NO_THROW(scene->readResampled4DBlockChannels(
        block, block.size(), {}, cv::Range(missingSlice, missingSlice + 1),
        cv::Range(0, 1), raster));
    ASSERT_EQ(raster.channels(), 3);

    std::vector<cv::Mat> channels;
    cv::split(raster, channels);
    EXPECT_EQ(TestTools::countNonZero(channels[missingChannel]), 0u)
        << "the channel with no image item should read as zeros";
    EXPECT_GT(TestTools::countNonZero(channels[0]), 0u);
    EXPECT_GT(TestTools::countNonZero(channels[2]), 0u);

    // A slice that is complete is unaffected.
    cv::Mat intact;
    ASSERT_NO_THROW(scene->readResampled4DBlockChannels(
        block, block.size(), {}, cv::Range(0, 1), cv::Range(0, 1), intact));
    std::vector<cv::Mat> intactChannels;
    cv::split(intact, intactChannels);
    EXPECT_GT(TestTools::countNonZero(intactChannels[missingChannel]), 0u);

    scene.reset();
    slide.reset();
    std::error_code ignored;
    std::filesystem::remove(patchedPath, ignored);
}

namespace
{
    // Rewrites a single-channel ZVI into a packed BGR one: the pixel format in
    // /Image/Contents and in the item header become PF_BGR, and the item height
    // is reduced so that width * height * 3 bytes still fit in the raster the
    // item already holds. No packed ZVI exists in the test set, and the packed
    // path -- one image item standing for three logical channels -- is not
    // reachable any other way.
    bool copyZviAsPackedBgr(const std::string& src, const std::string& dst,
                            int& packedWidth, int& packedHeight)
    {
        constexpr int32_t PF_BGR = 1;
        constexpr int32_t PACKED_CHANNELS = 3;

        CfbDirectory dir;
        if (!readCfbDirectory(src, dir)) return false;

        const size_t imageContents = findEntryByPath(dir, "/Image/Contents");
        const size_t itemContents = findEntryByPath(dir, "/Image/Item(0)/Contents");
        if (imageContents == dir.entryOffset.size() || itemContents == dir.entryOffset.size()) {
            return false;
        }

        // /Image/Contents: {4 items}{Width}{Height}{Depth}{PixelFormat}{RawCount}.
        {
            const std::vector<uint8_t> head = dir.readStream(imageContents, 256);
            size_t pos = 0;
            for (int i = 0; i < 4; ++i) pos = skipTypedItem(head, pos);
            pos = skipTypedItem(head, pos); // Width
            pos = skipTypedItem(head, pos); // Height
            pos = skipTypedItem(head, pos); // Depth
            if (pos + 6 > head.size()) return false;
            dir.writeI32(imageContents, pos + 2, PF_BGR);
        }

        // Item <Contents>: {11 items}{PositionInformation}{5 items}{7 int32 header}
        // where the header is {Version}{Width}{Height}{Depth}{?}{PixelFormat}{ValidBits}.
        {
            const std::vector<uint8_t> head = dir.readStream(itemContents, 1024);
            size_t pos = 0;
            for (int i = 0; i < 11; ++i) pos = skipTypedItem(head, pos);
            if (pos + 6 > head.size()) return false;
            pos += 2; // position blob type
            const uint32_t positionSize = CfbDirectory::u32(head, pos);
            pos += 4 + positionSize;
            for (int i = 0; i < 5; ++i) pos = skipTypedItem(head, pos);
            if (pos + 28 > head.size()) return false;

            const int32_t width = static_cast<int32_t>(CfbDirectory::u32(head, pos + 4));
            const int32_t storedHeight = static_cast<int32_t>(CfbDirectory::u32(head, pos + 8));
            if (width <= 0 || storedHeight <= 0) return false;
            // The raster is unchanged, so three bytes per pixel buys a third of
            // the rows of the two-byte-per-pixel original.
            const int32_t rasterBytes = width * storedHeight * 2;
            const int32_t height = rasterBytes / (width * PACKED_CHANNELS);
            if (height <= 0) return false;

            dir.writeI32(itemContents, pos + 8, height);
            dir.writeI32(itemContents, pos + 20, PF_BGR);
            packedWidth = width;
            packedHeight = height;
        }

        std::ofstream out(dst, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(reinterpret_cast<const char*>(dir.bytes.data()), dir.bytes.size());
        return out.good();
    }
}

// A packed pixel format stores every component of a pixel in one image item,
// so the scene's channels 1..n-1 have no item carrying their own channel
// index -- they are extracted from the item that carries channel 0. Looking
// them up by channel index alone finds nothing, and the packed channels come
// back blank.
TEST(ZVIImageDriver, readBlockPackedBgr)
{
    const std::string filePath = TestTools::getTestImagePath("zvi", "TOMMAlexaFluor647.zvi");
    const std::string packedPath =
        (std::filesystem::temp_directory_path() / "zvi-packed-bgr.zvi").string();
    int width = 0;
    int height = 0;
    ASSERT_TRUE(copyZviAsPackedBgr(filePath, packedPath, width, height))
        << "could not build the packed test file from " << filePath;
    ASSERT_GT(width, 0);
    ASSERT_GT(height, 0);

    slideio::ZVIImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(packedPath);
    ASSERT_TRUE(slide.get() != nullptr);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    ASSERT_TRUE(scene.get() != nullptr);

    // One image item, three logical channels.
    ASSERT_EQ(scene->getNumChannels(), 3);
    EXPECT_EQ(scene->getChannelName(0), std::string("blue"));
    EXPECT_EQ(scene->getChannelName(1), std::string("green"));
    EXPECT_EQ(scene->getChannelName(2), std::string("red"));
    EXPECT_EQ(scene->getChannelDataType(0), slideio::DataType::DT_Byte);

    const cv::Rect block(0, 0, std::min(400, width), std::min(300, height));
    cv::Mat raster;
    ASSERT_NO_THROW(scene->readBlock(block, raster));
    ASSERT_EQ(raster.channels(), 3);
    EXPECT_EQ(raster.depth(), CV_8U);

    std::vector<cv::Mat> channels;
    cv::split(raster, channels);
    // Every packed component has to come out of the raster. Before the packed
    // lookup existed, channels 1 and 2 resolved to no image item and read back
    // as zeros while channel 0 was correct.
    for (int channel = 0; channel < 3; ++channel) {
        EXPECT_GT(TestTools::countNonZero(channels[channel]), 0u)
            << "packed channel " << channel << " read back blank";
    }
    // The three components of a packed pixel differ; identical planes would
    // mean the same component was extracted three times.
    EXPECT_FALSE(TestTools::compareRastersEx(channels[0], channels[1]));

    scene.reset();
    slide.reset();
    std::error_code ignored;
    std::filesystem::remove(packedPath, ignored);
}

// An item whose tag stream cannot be read keeps the item -- the raster is in
// <Contents>, not in the tags -- but has no tile position. Resolving that
// position only in computeTiles' local variables left the item itself holding
// (-1,-1), which ZVITile::addItem read back and rejected as a coordinate
// mismatch against an item that had been added before it, aborting the open.
TEST(ZVIImageDriver, openSlideWithMissingItemTags)
{
    const std::string filePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Merged.zvi");
    const std::string patchedPath =
        (std::filesystem::temp_directory_path() / "zvi-missing-item-tags.zvi").string();
    // Item(0) is read first and supplies the tile position, so breaking a later
    // item is what produces the mismatch.
    const int brokenItem = 1;
    ASSERT_TRUE(copyZviWithoutItemTags(filePath, patchedPath, brokenItem))
        << "could not build the test file from " << filePath;

    slideio::ZVIImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide;
    ASSERT_NO_THROW(slide = driver.openFile(patchedPath));
    ASSERT_TRUE(slide.get() != nullptr);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    ASSERT_TRUE(scene.get() != nullptr);

    // The item is still there: its raster and its (c,z,t) come from <Contents>.
    EXPECT_EQ(scene->getNumChannels(), 3);
    EXPECT_EQ(scene->getRect().width, 1480);
    EXPECT_EQ(scene->getRect().height, 1132);
    // Only what the tag stream carried is lost.
    EXPECT_TRUE(scene->getChannelName(brokenItem).empty());
    EXPECT_EQ(scene->getChannelName(0), std::string("Hoechst 33342"));
    EXPECT_EQ(scene->getChannelName(2), std::string("FITC"));

    const cv::Rect block(0, 0, 400, 300);
    cv::Mat raster;
    ASSERT_NO_THROW(scene->readBlock(block, raster));
    ASSERT_EQ(raster.channels(), 3);
    std::vector<cv::Mat> channels;
    cv::split(raster, channels);
    for (int channel = 0; channel < 3; ++channel) {
        EXPECT_GT(TestTools::countNonZero(channels[channel]), 0u)
            << "channel " << channel << " read back blank";
    }

    scene.reset();
    slide.reset();
    std::error_code ignored;
    std::filesystem::remove(patchedPath, ignored);
}
