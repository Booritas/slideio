// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <fstream>
#include <boost/format.hpp>
#include <gtest/gtest.h>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>


#include "slideio/slideio/imagedrivermanager.hpp"
#include "testtools.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/core/cvtools.hpp"
#include "slideio/drivers/zvi/zviimagedriver.hpp"
#include "slideio/imagetools/imagetools.hpp"

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
        std::string channelName = (boost::format("Zeiss-1-Merged-ch%1%.tif") % channel).str();
        std::string channelPath = TestTools::getTestImagePath("zvi", channelName);
        slideio::ImageTools::readGDALImage(channelPath, channelRasterTest);

        cv::Mat channelDiff = cv::abs(channelRaster - channelRasterTest);
        double min(0), max(0);
        cv::minMaxLoc(channelDiff, &min, &max);
        EXPECT_EQ(min, 0);
        EXPECT_EQ(max, 0);
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
    slideio::ImageTools::readGDALImage(channelPath, channelRaster);
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
