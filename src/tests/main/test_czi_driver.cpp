﻿// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/slideio/imagedrivermanager.hpp"
#include "slideio/drivers/czi/cziimagedriver.hpp"
#include "slideio/drivers/czi/czislide.hpp"
#include "tests/testlib/testtools.hpp"


#include "slideio/core/tools/tools.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/base/exceptions.hpp"

TEST(CZIImageDriver, DriverManager_getDriverIDs)
{
    std::vector<std::string> driverIds = slideio::ImageDriverManager::getDriverIDs();
    auto it = std::find(driverIds.begin(),driverIds.end(), "CZI");
    EXPECT_FALSE(it==driverIds.end());
}
TEST(CZIImageDriver, getID)
{
    slideio::CZIImageDriver driver;
    std::string id = driver.getID();
    EXPECT_EQ(id,"CZI");
}

TEST(CZIImageDriver, canOpenFile)
{
    slideio::CZIImageDriver driver;
    EXPECT_TRUE(driver.canOpenFile("c:\\abbb\\a.czi"));
    EXPECT_FALSE(driver.canOpenFile("c:\\abbb\\a.czi.tmp"));
}

TEST(CZIImageDriver, openFile)
{
    slideio::CZIImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("czi","pJP31mCherry.czi");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide!=nullptr);
    int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    auto scene = slide->getScene(0);
    ASSERT_FALSE(scene == nullptr);
    auto sceneRect = scene->getRect();
    EXPECT_EQ(sceneRect.x, 0);
    EXPECT_EQ(sceneRect.y, 0);
    EXPECT_EQ(sceneRect.width, 512);
    EXPECT_EQ(sceneRect.height, 512);
    int numChannels = scene->getNumChannels();
    EXPECT_EQ(numChannels, 3);
    for(int channel=0; channel<numChannels; ++channel)
    {
        EXPECT_EQ(scene->getChannelDataType(channel), slideio::DataType::DT_Byte);
    }
    EXPECT_EQ(scene->getMagnification(), 100.);
    slideio::Resolution res = scene->getResolution();
    const double fileRes = 9.76783e-8;
    EXPECT_LT((100 * std::abs(res.x - fileRes) / fileRes), 1);
    EXPECT_LT((100 * std::abs(res.y - fileRes) / fileRes), 1);
}

TEST(CZIImageDriver, openFileInfo)
{
    slideio::CZIImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("czi", "08_18_2018_enc_1001_633.czi");
    std::string channelNames[] = {"646", "655", "664", "673", "682", "691"};
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    auto scene = slide->getScene(0);
    ASSERT_FALSE(scene == nullptr);
    auto sceneRect = scene->getRect();
    EXPECT_EQ(sceneRect.x, 0);
    EXPECT_EQ(sceneRect.y, 0);
    EXPECT_EQ(sceneRect.width, 1000);
    EXPECT_EQ(sceneRect.height, 1000);
    int numChannels = scene->getNumChannels();
    EXPECT_EQ(numChannels, 6);
    for (int channelIndex = 0; channelIndex < numChannels; ++channelIndex)
    {
        std::string channelName = scene->getChannelName(channelIndex);
        EXPECT_EQ(channelName, channelNames[channelIndex]);
        EXPECT_EQ(scene->getChannelDataType(channelIndex), slideio::DataType::DT_UInt16);
    }
    EXPECT_EQ(scene->getMagnification(), 63.);
    slideio::Resolution res = scene->getResolution();
    const double fileRes = 6.7475572821478794e-008;
    EXPECT_LT((100 * std::abs(res.x - fileRes) / fileRes), 1);
    EXPECT_LT((100 * std::abs(res.y - fileRes) / fileRes), 1);
    EXPECT_STREQ(scene->getChannelName(2).c_str(), "664");
    std::string sceneName = scene->getName();
}

TEST(CZIImageDriver, readBlock)
{
    slideio::CZIImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("czi","pJP31mCherry.czi");
    std::string channelBmps[] = {
        TestTools::getTestImagePath("czi","pJP31mCherry.grey/pJP31mCherry_b0t0z0c0x0-512y0-512.bmp"),
        TestTools::getTestImagePath("czi","pJP31mCherry.grey/pJP31mCherry_b0t0z0c1x0-512y0-512.bmp"),
        TestTools::getTestImagePath("czi","pJP31mCherry.grey/pJP31mCherry_b0t0z0c2x0-512y0-512.bmp")
    };
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide!=nullptr);
    int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    auto scene = slide->getScene(0);
    ASSERT_FALSE(scene == nullptr);
    auto sceneRect = scene->getRect();
    for(int channelIndex=0; channelIndex<3; ++channelIndex)
    {
        // read channel raster
        cv::Mat raster;
        std::vector<int> channelIndices = {channelIndex};
        scene->readBlockChannels(sceneRect,channelIndices,raster);
        // read exported bmp channel
        cv::Mat bmpImage; // = cv::imread(channelBmps[channelIndex], cv::IMREAD_GRAYSCALE);
        slideio::ImageTools::readGDALImage(channelBmps[channelIndex], bmpImage);
        cv::Mat channelImage;
        cv::extractChannel(bmpImage, channelImage, 0);
        // compare equality of rasters from bmp and czi file
        int compare = std::memcmp(raster.data, channelImage.data, raster.total()*raster.elemSize());
        EXPECT_EQ(compare, 0);
    }
}

TEST(CZIImageDriver, readBlockStrongDownscaleNotThrowing)
{
    slideio::CZIImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("czi", "PYP-467.czi");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    auto scene = slide->getScene(0);
    ASSERT_FALSE(scene == nullptr);
    auto sceneRect = scene->getRect();
    cv::Size blockSize(5, 5);
    cv::Rect blockRect(2148, 0, 40, 40);
    cv::Mat mat;
    scene->readResampledBlock(blockRect, blockSize, mat);
    //TestTools::showRaster(mat);
}

TEST(CZIImageDriver, readBlock4D)
{
    slideio::CZIImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("czi","pJP31mCherry.czi");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide!=nullptr);
    int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    auto scene = slide->getScene(0);
    ASSERT_FALSE(scene == nullptr);
    auto sceneRect = scene->getRect();
    cv::Range zSliceRange = {2, 5};
    for(int channelIndex=0; channelIndex<3; ++channelIndex)
    {
        // read channel raster
        cv::Mat raster;
        std::vector<int> channelIndices = {channelIndex};
        scene->readResampled4DBlockChannels(sceneRect, sceneRect.size(), channelIndices,
            {2,5}, {0,1}, raster);
        EXPECT_EQ(raster.channels(), channelIndices.size());
        EXPECT_EQ(raster.size[0], sceneRect.width);
        EXPECT_EQ(raster.size[1], sceneRect.height);
        EXPECT_EQ(raster.size[2], 3);
        for(int zSliceIndex=zSliceRange.start; zSliceIndex<zSliceRange.end; ++zSliceIndex)
        {
            cv::Mat sliceRaster;
            slideio::CVTools::extractSliceFrom3D(raster, zSliceIndex - zSliceRange.start, sliceRaster);
            std::string bmpFileName =
            std::string("pJP31mCherry.grey/pJP31mCherry_b0t0z") +
            std::to_string(zSliceIndex) +
            std::string("c") +
            std::to_string(channelIndex) +
            std::string("x0-512y0-512.bmp");
            std::string bmpFilePath = TestTools::getTestImagePath("czi",bmpFileName);
            // read exported bmp channel
            cv::Mat bmpImage; // = cv::imread(bmpFilePath, cv::IMREAD_GRAYSCALE);
            slideio::ImageTools::readGDALImage(bmpFilePath, bmpImage);
            cv::Mat channelImage;
            cv::extractChannel(bmpImage, channelImage, 0);
            int compare = std::memcmp(sliceRaster.data, channelImage.data, sliceRaster.total()*sliceRaster.elemSize());
            EXPECT_EQ(compare, 0);
        }
    }
}

TEST(CZIImageDriver, sceneId)
{
    {
        int values[] = { 1, 2, 3, 4, 5, 6 };
        int values2[6] = { 0 };
        uint64_t sceneId = slideio::CZIScene::sceneIdFromDims(values[0], values[1], values[2], values[3], values[4], values[5]);
        slideio::CZIScene::dimsFromSceneId(sceneId, values2[0], values2[1], values2[2], values2[3], values2[4], values2[5]);
        for (int val = 0; val < 6; val++)
        {
            EXPECT_EQ(values[val], values2[val]);
        }
    }
    {
        int values[] = { 1, 0, 0, 0, 0, 0 };
        int values2[6] = { 0 };
        uint64_t sceneId = slideio::CZIScene::sceneIdFromDims(values[0], values[1], values[2], values[3], values[4], values[5]);
        slideio::CZIScene::dimsFromSceneId(sceneId, values2[0], values2[1], values2[2], values2[3], values2[4], values2[5]);
        for (int val = 0; val < 6; val++)
        {
            EXPECT_EQ(values[val], values2[val]);
        }
    }
    {
        int values[] = { 0, 0, 4, 0, 0, 0 };
        int values2[6] = { 0 };
        uint64_t sceneId = slideio::CZIScene::sceneIdFromDims(values[0], values[1], values[2], values[3], values[4], values[5]);
        slideio::CZIScene::dimsFromSceneId(sceneId, values2[0], values2[1], values2[2], values2[3], values2[4], values2[5]);
        for (int val = 0; val < 6; val++)
        {
            EXPECT_EQ(values[val], values2[val]);
        }
    }
    {
        int values[] = { 0, 0, 0, 0, 10, 0 };
        int values2[6] = { 0 };
        uint64_t sceneId = slideio::CZIScene::sceneIdFromDims(values[0], values[1], values[2], values[3], values[4], values[5]);
        slideio::CZIScene::dimsFromSceneId(sceneId, values2[0], values2[1], values2[2], values2[3], values2[4], values2[5]);
        for (int val = 0; val < 6; val++)
        {
            EXPECT_EQ(values[val], values2[val]);
        }
    }
}
TEST(CZIImageDriver, sceneIdsFromDims)
{
    {
        std::vector<slideio::Dimension> dims = {
            {'V',1,1},
            {'H',2,1},
            {'I',3,1},
            {'R',4,1},
            {'B',5,1},
            {'S',6,1},
        };
        std::vector<uint64_t> ids;
        slideio::CZIScene::sceneIdsFromDims(dims, ids);
        auto sceneId = slideio::CZIScene::sceneIdFromDims(6,3,1,2,4,5);
        EXPECT_EQ(ids.size(),1);
        EXPECT_EQ(ids[0], sceneId);
    }
    {
        std::vector<slideio::Dimension> dims = {
            {'V',1,1},
            {'H',2,1},
            {'I',3,1},
            {'R',4,1},
            {'B',5,1},
            {'S',6,2},
        };
        std::vector<uint64_t> ids;
        slideio::CZIScene::sceneIdsFromDims(dims, ids);
        auto sceneId1 = slideio::CZIScene::sceneIdFromDims(6,3,1,2,4,5);
        auto sceneId2 = slideio::CZIScene::sceneIdFromDims(7,3,1,2,4,5);
        EXPECT_EQ(ids.size(),2);
        EXPECT_EQ(ids[0], sceneId1);
        EXPECT_EQ(ids[1], sceneId2);
    }
    {
        std::vector<slideio::Dimension> dims = {
            {'V',1,2},
            {'S',6,2},
        };
        std::vector<uint64_t> ids;
        slideio::CZIScene::sceneIdsFromDims(dims, ids);
        auto sceneId1 = slideio::CZIScene::sceneIdFromDims(6,0,1,0,0,0);
        auto sceneId2 = slideio::CZIScene::sceneIdFromDims(6,0,2,0,0,0);
        auto sceneId3 = slideio::CZIScene::sceneIdFromDims(7,0,1,0,0,0);
        auto sceneId4 = slideio::CZIScene::sceneIdFromDims(7,0,2,0,0,0);
        EXPECT_EQ(ids.size(),4);
        EXPECT_TRUE(std::find(ids.begin(), ids.end(),sceneId1)!=ids.end());
        EXPECT_TRUE(std::find(ids.begin(), ids.end(),sceneId2)!=ids.end());
        EXPECT_TRUE(std::find(ids.begin(), ids.end(),sceneId3)!=ids.end());
        EXPECT_TRUE(std::find(ids.begin(), ids.end(),sceneId4)!=ids.end());
    }
    {
        std::vector<slideio::Dimension> dims = {
            {'V',1,2},
            {'H',2,2},
            {'I',3,2},
            {'R',4,2},
            {'B',5,2},
            {'S',6,2},
        };
        std::vector<uint64_t> ids;
        slideio::CZIScene::sceneIdsFromDims(dims, ids);
        EXPECT_EQ(ids.size(),64);
    }
}

static void testChannelNames(const std::string& imageName, int sceneIndex, const std::vector<std::string>& channelNames)
{
    slideio::CZIImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("czi",imageName);
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide!=nullptr);
    int numScenes = slide->getNumScenes();
    ASSERT_GT(numScenes, sceneIndex);
    auto scene = slide->getScene(sceneIndex);
    ASSERT_FALSE(scene == nullptr);
    const size_t numChannels = scene->getNumChannels();
    ASSERT_EQ(numChannels, channelNames.size());
    for(int channelIndex=0; channelIndex<numChannels; ++channelIndex)
    {
        const std::string channelName = scene->getChannelName(channelIndex);
        EXPECT_EQ(channelNames[channelIndex], channelName);
    }
}

TEST(CZIImageDriver, channelNames)
{
    {
        std::string image_name("03_14_2019_DSGN0545_A_wb_1353_fov_1_633.czi");
        std::vector<std::string> channelNames = {"646","655","664", "673", "682", "691"};
        testChannelNames(image_name, 0, channelNames);
    }
    {
        std::string image_name("pJP31mCherry.czi");
        std::vector<std::string> channelNames = {"ChS1","Ch2","NDD T1"};
        testChannelNames(image_name, 0, channelNames);
    }
}

TEST(CZIImageDriver, slideRawMetadata)
{
    const std::string images[] = {
        "03_14_2019_DSGN0545_A_wb_1353_fov_1_633.czi",
        "pJP31mCherry.czi"
    };
    slideio::CZIImageDriver driver;
    for(const auto& imageName: images)
    {
        std::string filePath = TestTools::getTestImagePath("czi",imageName);
        std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
        const std::string& metadata = slide->getRawMetadata();
        EXPECT_GT(metadata.length(),0);
        const std::string header("<ImageDocument>");
        EXPECT_TRUE(TestTools::starts_with(metadata, header));
    }
}

TEST(CZIImageDriver, metadataCompression)
{
    const std::string images[] = {
        "03_14_2019_DSGN0545_A_wb_1353_fov_1_633.czi",
        "pJP31mCherry.czi", "test2.czi"
    };
    typedef std::tuple<int, slideio::Compression> SceneCompression;
    const SceneCompression compression[] ={
        SceneCompression(0,slideio::Compression::Uncompressed),
        SceneCompression(0, slideio::Compression::Uncompressed),
    };
    const int itemCount = sizeof(compression)/sizeof(compression[0]);

    slideio::CZIImageDriver driver;
    for(int item=0; item<itemCount; ++item)
    {
        const std::string& imageName = images[item];
        const SceneCompression& compr = compression[item];

        std::string filePath = TestTools::getTestImagePath("czi",imageName);
        std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
        const int sceneIndex = std::get<0>(compr);
        const slideio::Compression sceneCompression = std::get<1>(compr);
        std::shared_ptr<slideio::CVScene> scene = slide->getScene(sceneIndex);
        EXPECT_TRUE(scene!=nullptr);
        EXPECT_EQ(scene->getCompression(), sceneCompression);
    }
}

TEST(CZIImageDriver, crashTestNotCZIImage)
{
    std::string filePath = TestTools::getTestImagePath("svs","corrupted.svs");
    slideio::CZIImageDriver driver;
    EXPECT_THROW(driver.openFile(filePath),slideio::RuntimeError);
}

TEST(CZIImageDriver, corruptedCZI)
{
    std::string filePath = TestTools::getTestImagePath("czi","corrupted.czi");
    slideio::CZIImageDriver driver;
    EXPECT_THROW(driver.openFile(filePath), std::exception);
}

static void testAuxImage(const std::string& imagePath, const std::string& auxImageName, const std::string testImagePath)
{
    slideio::CZIImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(imagePath);
    ASSERT_TRUE(slide != nullptr);
    std::shared_ptr<slideio::CVScene> auxScene = slide->getAuxImage(auxImageName);
    cv::Rect rect = auxScene->getRect();
    cv::Mat auxRaster;
    rect.x = rect.y = 0;
    auxScene->readBlock(rect, auxRaster);
    ASSERT_EQ(auxRaster.size().width, rect.width);
    ASSERT_EQ(auxRaster.size().height, rect.height);

    cv::Mat testRaster;
    slideio::ImageTools::readGDALImage(testImagePath, testRaster);
    double score = slideio::ImageTools::computeSimilarity2(auxRaster, testRaster);
    ASSERT_GT(score, 0.99);
}

static void writeAuxImage(const std::string& imagePath, const std::string& auxImageName, const std::string testImagePath)
{
   slideio::CZIImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(imagePath);
    ASSERT_TRUE(slide != nullptr);
    std::shared_ptr<slideio::CVScene> auxScene = slide->getAuxImage(auxImageName);
    cv::Rect rect = auxScene->getRect();
    cv::Mat auxRaster;
    rect.x = rect.y = 0;
    auxScene->readBlock(rect, auxRaster);
    ASSERT_EQ(auxRaster.size().width, rect.width);
    ASSERT_EQ(auxRaster.size().height, rect.height);

    slideio::ImageTools::writeTiffImage(testImagePath, auxRaster);
}

TEST(CZIImageDriver, auxSlidePreview)
{
    if (!TestTools::isPrivateTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because private dataset is not enabled";
    }
    std::string imagePath = TestTools::getTestImagePath("czi", "jxr-rgb-5scenes.czi", true);
    std::string testImagePath = TestTools::getTestImagePath("czi", "jxr-rgb-5scenes.preview.tiff", true);
    testAuxImage(imagePath, "SlidePreview", testImagePath);
}

TEST(CZIImageDriver, auxSlidePreviewTimeFrame)
{
    if (!TestTools::isPrivateTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because private dataset is not enabled";
    }
    std::string imagePath = TestTools::getTestImagePath("czi", "jxr-16bit-4chnls.czi", true);
    std::string testImagePath = TestTools::getTestImagePath("czi", "jxr-16bit-4chnls.preview.tiff", true);
    testAuxImage(imagePath, "SlidePreview", testImagePath);
}

TEST(CZIImageDriver, auxThumbnail)
{
    if (!TestTools::isPrivateTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because private dataset is not enabled";
    }
    std::string imagePath = TestTools::getTestImagePath("czi", "jxr-16bit-4chnls.czi", true);
    std::string testImagePath = TestTools::getTestImagePath("czi", "jxr-16bit-4chnls.thumb.png", true);
    testAuxImage(imagePath, "Thumbnail", testImagePath);
}

TEST(CZIImageDriver, auxThumbnail2)
{
    if (!TestTools::isPrivateTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because private dataset is not enabled";
    }
    std::string imagePath = TestTools::getTestImagePath("czi", "jxr-rgb-5scenes.czi", true);
    std::string testImagePath = TestTools::getTestImagePath("czi", "jxr-rgb-5scenes.thumb.png", true);
    testAuxImage(imagePath, "Thumbnail", testImagePath);
}

TEST(CZIImageDriver, auxLabel)
{
    if (!TestTools::isPrivateTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because private dataset is not enabled";
    }
    if (!TestTools::isPrivateTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because private dataset is not enabled";
    }
    std::string imagePath = TestTools::getTestImagePath("czi", "jxr-rgb-5scenes.czi", true);
    std::string testImagePath = TestTools::getTestImagePath("czi", "jxr-rgb-5scenes.label.tiff", true);
    testAuxImage(imagePath, "Label", testImagePath);
}

TEST(CZIImageDriver, timeResolution)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }
    std::string imagePath = TestTools::getFullTestImagePath("czi", "T_3_CH_2.czi");
    slideio::CZIImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(imagePath);
    ASSERT_TRUE(slide != nullptr);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    auto res = scene->getTFrameResolution();
    ASSERT_DOUBLE_EQ(res, 0.0615);
}

TEST(CZIImageDriver, mosaicFile)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }
    std::string imagePath = TestTools::getFullTestImagePath("czi", "16bit_CH_1_doughnut_crop.czi");
    slideio::CZIImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(imagePath);
    ASSERT_TRUE(slide != nullptr);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    auto rect = scene->getRect();
    ASSERT_EQ(rect.width, 498);
    ASSERT_EQ(rect.height, 266);
    cv::Mat raster;
    scene->readBlock(rect, raster);
    std::string testImagePath = TestTools::getFullTestImagePath("czi", "test/16bit_CH_1_doughnut_crop.tiff");
    cv::Mat testRaster;
    slideio::ImageTools::readGDALImage(testImagePath, testRaster);
    auto memSize = raster.total() * raster.elemSize();
    ASSERT_EQ(memcmp(raster.data, testRaster.data, memSize),0);
}

TEST(CZIImageDriver, artificialFile)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }
    std::string imagePath = TestTools::getFullTestImagePath("czi", "bug_2D_rgb_compressed.czi");
    slideio::CZIImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(imagePath);
    ASSERT_TRUE(slide != nullptr);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    auto rect = scene->getRect();
    ASSERT_EQ(rect.width, 975);
    ASSERT_EQ(rect.height, 918);
    cv::Mat raster;
    scene->readBlock(rect, raster);
    std::string testImagePath = TestTools::getFullTestImagePath("czi", "test/bug_2D_rgb_compressed.png");
    cv::Mat testRaster;
    slideio::ImageTools::readGDALImage(testImagePath, testRaster);
    auto memSize = raster.total() * raster.elemSize();
    ASSERT_EQ(memcmp(raster.data, testRaster.data, memSize), 0);
}


TEST(CZIImageDriver, mozaicZoomPyramid)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }
    std::string imagePath = TestTools::getFullTestImagePath("czi", "zeiss.czi");
    std::string testImagePath = TestTools::getFullTestImagePath("czi", "test/zeiss-block.png");
    slideio::CZIImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(imagePath);
    ASSERT_TRUE(slide != nullptr);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    auto rect = scene->getRect();
    ASSERT_EQ(rect.height, 7673);
    ASSERT_EQ(rect.width, 46232);
    cv::Mat raster;
    cv::Rect blockRect = { 9350, 1000, 3300, 3000 };
    cv::Size blockSize = blockRect.size();
    blockSize.width /= 3;
    blockSize.height /= 3;
    scene->readResampledBlock(blockRect, blockSize, raster);
    cv::Mat testRaster;
    slideio::ImageTools::readGDALImage(testImagePath, testRaster);
    auto memSize = raster.total() * raster.elemSize();
    ASSERT_EQ(memcmp(raster.data, testRaster.data, memSize), 0);
}


TEST(CZIImageDriver, openDamagedFile)
{
    slideio::CZIImageDriver driver;

    std::string filePath = TestTools::getFullTestImagePath("czi", "private/E2_A3_W12.czi");

    ASSERT_NO_THROW(driver.openFile(filePath));
}

TEST(CZIImageDriver, auxSceneMemoryReallocatedBug)
{
    std::string imagePath = TestTools::getTestImagePath("czi", "03_14_2019_DSGN0545_A_wb_1353_fov_1_633.czi");
    slideio::CZIImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(imagePath);
    ASSERT_TRUE(slide != nullptr);
    std::list<std::string> imageNames = slide->getAuxImageNames();
    const int maxWidth = 500;
    for(auto & imageName: imageNames) {
        std::shared_ptr<slideio::CVScene> auxImage = slide->getAuxImage(imageName);
        cv::Mat auxRaster;
        cv::Rect sceneRect = auxImage->getRect();
        double cof = 500. / sceneRect.width;
        cv::Size size(500, std::lround(cof * sceneRect.height));
        std::vector<int> channels = {2,1,0};
        auxImage->readResampledBlockChannels(sceneRect, size, channels, auxRaster);
    }
}

TEST(CZIImageDriver, openFileUtf8)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }
    {
        std::string filePath = TestTools::getFullTestImagePath("unicode", u8"тест/pJP31mCherry.czi");
        slideio::CZIImageDriver driver;
        std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
        int dirCount = slide->getNumScenes();
        ASSERT_EQ(dirCount, 1);
        std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
        auto rect = scene->getRect();
        cv::Rect expectedRect(0, 0, 512, 512);
        EXPECT_EQ(rect, expectedRect);
        cv::Mat raster;
        rect.x = rect.y = 0;
        scene->readBlock(rect, raster);
        EXPECT_EQ(raster.cols, rect.width);
        EXPECT_EQ(raster.rows, rect.height);
    }
}

TEST(CZIImageDriver, zoomLevels)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }
    const slideio::LevelInfo levels[] = {
        slideio::LevelInfo(0, {49132,48722}, 1.0, 40., {1600,1200}),
        slideio::LevelInfo(1, {24566,24361}, 0.5, 20, {1024,1024}),
        slideio::LevelInfo(2, {12283,12181}, 0.25, 10, {1024,1024}),
        slideio::LevelInfo(3, {6142,6090}, 0.125, 5, {1024,1024}),
        slideio::LevelInfo(4, {3071,3045}, 0.0625, 2.5, {1024,1024}),
        slideio::LevelInfo(5, {1535,1523}, 0.03117, 1.25, {511,1024}),
        slideio::LevelInfo(6, {768,761}, 0.015625, 0.625, {768,762}),
    };
    slideio::CZIImageDriver driver;
    std::string filePath = TestTools::getFullTestImagePath("czi", "30-10-2020_NothingRecognized-15986.czi");
    const std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    const std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    const int numScenes = slide->getNumScenes();
    const cv::Rect rect = scene->getRect();
    double magnification = scene->getMagnification();
    ASSERT_TRUE(scene != nullptr);
    const int numLevels = scene->getNumZoomLevels();
    ASSERT_EQ(7, numLevels);
    for (int levelIndex = 0; levelIndex < numLevels; ++levelIndex)
    {
        const slideio::LevelInfo* level = scene->getZoomLevelInfo(levelIndex);
        EXPECT_EQ(*level, levels[levelIndex]);
        if (levelIndex == 0) {
            EXPECT_EQ(level->getSize(), slideio::Tools::cvSizeToSize(rect.size()));
        }

    }
}

TEST(CZIImageDriver, multiThreadSceneAccess) {
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() <<
            "Skip the test because full dataset is not enabled";
    }
    std::string filePath = TestTools::getTestImagePath("czi", "03_14_2019_DSGN0545_A_wb_1353_fov_1_633.czi");
    slideio::CZIImageDriver driver;
    TestTools::multiThreadedTest(filePath, driver);
}
