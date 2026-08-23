// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <filesystem>
#include <gtest/gtest.h>
#include "slideio/slideio/imagedrivermanager.hpp"
#include "slideio/drivers/czi/cziimagedriver.hpp"
#include "slideio/drivers/czi/czislide.hpp"
#include "slideio/drivers/czi/czisubblock.hpp"
#include "tests/testlib/testtools.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/slideio/slideio.hpp"

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

TEST(CZIImageDriver, computeLevelZoom)
{
    const double epsilon = 1.e-9;
    // sub-blocks of one pyramid level are assigned the zoom of the level, whether their
    // logical extent is divisible by the downsample factor or not
    EXPECT_NEAR(1., slideio::CZISubBlock::computeLevelZoom(1024, 1024), epsilon);
    EXPECT_NEAR(0.5, slideio::CZISubBlock::computeLevelZoom(1024, 512), epsilon);
    EXPECT_NEAR(0.5, slideio::CZISubBlock::computeLevelZoom(1025, 513), epsilon); // rounded up
    EXPECT_NEAR(0.5, slideio::CZISubBlock::computeLevelZoom(1025, 512), epsilon); // rounded down
    // ratios taken from affected Zeiss slides; 1213/304 also guards against the size/downsample
    // division degrading to integer division, which would reject the nominal factor
    EXPECT_NEAR(0.25, slideio::CZISubBlock::computeLevelZoom(1213, 304), epsilon);
    EXPECT_NEAR(0.125, slideio::CZISubBlock::computeLevelZoom(6932, 867), epsilon);
    EXPECT_NEAR(0.0625, slideio::CZISubBlock::computeLevelZoom(15124, 946), epsilon);
    EXPECT_NEAR(1. / 3., slideio::CZISubBlock::computeLevelZoom(2049, 683), epsilon);
    EXPECT_NEAR(1. / 3., slideio::CZISubBlock::computeLevelZoom(2050, 683), epsilon);
    // scaling that no integer downsample factor explains is kept as it is
    EXPECT_NEAR(0.137, slideio::CZISubBlock::computeLevelZoom(1000, 137), epsilon);
    EXPECT_NEAR(0.6, slideio::CZISubBlock::computeLevelZoom(1000, 600), epsilon);
    EXPECT_NEAR(1.5, slideio::CZISubBlock::computeLevelZoom(100, 150), epsilon);
    EXPECT_NEAR(1., slideio::CZISubBlock::computeLevelZoom(0, 10), epsilon);
    EXPECT_NEAR(1., slideio::CZISubBlock::computeLevelZoom(1000, 0), epsilon);
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
        slideio::ImageTools::readSmallImageRaster(channelBmps[channelIndex], bmpImage);
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
            slideio::ImageTools::readSmallImageRaster(bmpFilePath, bmpImage);
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
		EXPECT_EQ(slide->getMetadataFormat(), slideio::MetadataFormat::XML);
		EXPECT_EQ(slide->getScene(0)->getMetadataFormat(), slideio::MetadataFormat::None);
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
    slideio::ImageTools::readSmallImageRaster(testImagePath, testRaster);
    double score = slideio::ImageTools::computeSimilarity2(auxRaster, testRaster);
    ASSERT_GT(score, 0.99);
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
    slideio::ImageTools::readSmallImageRaster(testImagePath, testRaster);
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
    slideio::ImageTools::readSmallImageRaster(testImagePath, testRaster);
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
    slideio::ImageTools::readSmallImageRaster(testImagePath, testRaster);
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

TEST(CZIImageDriver, channelAttributes)
{
    slideio::CZIImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("czi", "pJP31mCherry.czi");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    auto scene = slide->getScene(0);
    ASSERT_FALSE(scene == nullptr);
    const slideio::Metadata& chanAttrs = scene->getChannelAttributes();
	ASSERT_EQ(chanAttrs.size(), 3u);                  // numChannels for this scene
	EXPECT_EQ(chanAttrs[0].size(), 17u);              // channel 0 has the 17 distinct attribute names
	EXPECT_TRUE(chanAttrs[0].contains("Name"));
    EXPECT_EQ(chanAttrs[0]["Name"].asString(),                "ChS1");
    EXPECT_EQ(chanAttrs[1]["Name"].asString(),                "Ch2");
    EXPECT_EQ(chanAttrs[2]["Name"].asString(),                "NDD T1");
    EXPECT_EQ(chanAttrs[0]["EmissionWavelength"].asString(),  "610.63882650000005");
    EXPECT_EQ(chanAttrs[0]["ChannelType"].asString(),         "Unspecified");
    EXPECT_EQ(chanAttrs[1]["PinholeSizeAiry"].asString(),     "1");
    EXPECT_EQ(chanAttrs[0]["AcquisitionMode"].asString(),     "LaserScanningConfocalMicroscopy");
}

/**
 * 
 */
TEST(CZIImageDriver, channelAttributes2)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }
    {
        std::string imagePath = TestTools::getFullTestImagePath("czi", "bug_2D_rgb_compressed.czi");
        slideio::CZIImageDriver driver;
        std::shared_ptr<slideio::CVSlide> slide = driver.openFile(imagePath);
        ASSERT_TRUE(slide != nullptr);
        std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
        const slideio::Metadata& metadata = scene->getChannelAttributes();
        EXPECT_TRUE(metadata.isArray());
        EXPECT_EQ(metadata.size(), 3);
		EXPECT_EQ(metadata[0]["Color"].asString(), "#0000FF");
        EXPECT_EQ(metadata[1]["Color"].asString(), "#00FF00");
        EXPECT_EQ(metadata[2]["Color"].asString(), "#FF0000");
    }
    {
        std::string imagePath = TestTools::getFullTestImagePath("czi", "private/20-024_K5_HE.czi");
        slideio::CZIImageDriver driver;
        std::shared_ptr<slideio::CVSlide> slide = driver.openFile(imagePath);
        ASSERT_TRUE(slide != nullptr);
        std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
        const slideio::Metadata& metadata = scene->getChannelAttributes();
        EXPECT_TRUE(metadata.isArray());
        EXPECT_EQ(metadata.size(), 3);
        EXPECT_EQ(metadata[0]["Color"].asString(), "#0000FF");
        EXPECT_EQ(metadata[1]["Color"].asString(), "#00FF00");
        EXPECT_EQ(metadata[2]["Color"].asString(), "#FF0000");
    }
}

TEST(CZIImageDriver, getDriverId)
{
    std::string filePath = TestTools::getTestImagePath("czi", "pJP31mCherry.czi");
    auto slide = slideio::openSlide(filePath, "AUTO");
    ASSERT_TRUE(slide);
    EXPECT_EQ("CZI", slide->getDriverId());
    const int numScenes = slide->getNumScenes();
    EXPECT_EQ(1, numScenes);
    for (int iScene = 0; iScene < numScenes; ++iScene) {
        std::shared_ptr<slideio::CVScene> scene = slide->getScene(iScene)->getCVScene();
        EXPECT_TRUE(scene.get() != nullptr);
        EXPECT_EQ(iScene, scene->getSceneIndex());
        EXPECT_EQ(filePath, scene->getFilePath());
		EXPECT_EQ("CZI", scene->getDriverId());
    }
}

TEST(CZIImageDriver, openChannelColor)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }
    {
        std::string filePath = TestTools::getFullTestImagePath("czi", u8"openslide/Zeiss-4-Mosaic.czi");
        slideio::CZIImageDriver driver;
        std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
        int dirCount = slide->getNumScenes();
        ASSERT_EQ(dirCount, 1);
        std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
        const slideio::Metadata& chanAttrs = scene->getChannelAttributes();
		ASSERT_EQ(chanAttrs.size(), 3u); // numChannels for this scene
        EXPECT_TRUE(chanAttrs[0].contains("Color"));
        EXPECT_EQ(chanAttrs[0]["Color"].asString(), "#FF0000FF");
        EXPECT_EQ(chanAttrs[1]["Color"].asString(), "#FF00FF00");
		EXPECT_EQ(chanAttrs[2]["Color"].asString(), "#FFFF0000");
    }
}

TEST(CZIImageDriver, splitZoomLevel)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }
    std::string filePath = TestTools::getFullTestImagePath("czi", u8"private/example_split.czi");
    std::string roiPaths[] = {
        TestTools::getFullTestImagePath("czi", "test/example_split (1).czi - ScanRegion0 (1, x=17583, y=3676, w=1000, h=1000).png"),
        TestTools::getFullTestImagePath("czi", "test/example_split (1).czi - ScanRegion0 (1, x=41169, y=4850, w=1000, h=1000).png"),
        TestTools::getFullTestImagePath("czi", "test/example_split (1).czi - ScanRegion0 (1, x=2668, y=1376, w=1000, h=1000).png"),
    };
    slideio::CZIImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    int dirCount = slide->getNumScenes();
    ASSERT_EQ(dirCount, 1);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
	cv::Rect sceneRect = scene->getRect();
    constexpr int blockWidth = 1000;
	constexpr int blockHeight = 1000;
    cv::Rect rects[] = {
        {17583, 3676, blockWidth, blockHeight},
        {41169, 4850, blockWidth, blockHeight},
        {2668, 1376, blockWidth, blockHeight}
    };
    constexpr double downscale = 2.;
	cv::Size resampledSize(std::lround(blockWidth/downscale), std::lround(blockHeight/downscale));
    for (size_t i = 0; i < std::size(rects); ++i) {
        const cv::Rect& rect = rects[i];
        cv::Size size(static_cast<int>(rect.width / downscale), static_cast<int>(rect.height / downscale));
        cv::Mat block;
        scene->readResampledBlockChannels(rect, size, {2,1,0}, block);
        cv::Mat testRaster;
        slideio::ImageTools::readSmallImageRaster(roiPaths[i], testRaster);
        cv::Mat resampledRaster;
        cv::resize(testRaster, resampledRaster, resampledSize, 0., 0., cv::INTER_NEAREST);
        //TestTools::showRasters(block, resampledRaster);
        double sim = slideio::ImageTools::computeSimilarity2(block, resampledRaster);
        //cv::Mat diff;
        //cv::absdiff(block, resampledRaster, diff);
        //TestTools::showRaster(diff);
        EXPECT_NEAR(sim, 1.0, 0.06);
    }
}

// Behavior preservation: a level read resampled down to a coarser level's size must
// essentially match a scene read resampled to the same size, level by level.
TEST(CZIImageDriver, readLevelMatchesTheResampledSceneRead)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }
    std::string filePath = TestTools::getFullTestImagePath("czi", "zeiss.czi");
    slideio::CZIImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);

    const int numLevels = scene->getNumZoomLevels();
    ASSERT_LE(2, numLevels);
    // computeSimilarity2 goes through cv::sum, which only supports up to 4 channels; cap the
    // comparison to a channel subset if the fixture turns out to carry more.
    const int numChannels = scene->getNumChannels();
    const std::vector<int> channelIndices = numChannels > 4 ? std::vector<int>{0, 1, 2} : std::vector<int>{};
    // This fixture's getRect() reports the sub-block union in raw file coordinates, which for
    // a mosaic scan is not zero-based (its origin here is a large negative offset); the tile
    // grid a block read actually addresses is zero-based, so the scene-space comparison rect
    // has to be zero-based too, matching the level-space rect used for the level read.
    const cv::Rect zeroBasedSceneRect(cv::Point(0, 0), scene->getRect().size());
    for (int level = std::max(1, numLevels - 3); level < numLevels; ++level)
    {
        const slideio::LevelInfo* info = scene->getZoomLevelInfo(level);
        ASSERT_TRUE(info != nullptr) << "level " << level;
        const cv::Size levelSize(info->getSize().width, info->getSize().height);
        cv::Mat viaLevel, viaScene;
        scene->readResampledLevelBlockChannels(level, cv::Rect(cv::Point(0, 0), levelSize), levelSize,
                                               channelIndices, viaLevel);
        scene->readResampledBlockChannels(zeroBasedSceneRect, levelSize, channelIndices, viaScene);
        ASSERT_EQ(levelSize, viaLevel.size()) << "level " << level;
        EXPECT_LE(0.95, slideio::ImageTools::computeSimilarity2(viaLevel, viaScene)) << "level " << level;
    }

    // zeiss.czi has a single z slice, so the 4D path is exercised separately below against a
    // fixture that actually has more than one.
    ASSERT_EQ(1, scene->getNumZSlices());

    // The level path assembles slices through the same helper as the scene path, so a single
    // slice read has to agree with the corresponding plane of the scene read.
    // 30-10-2020_NothingRecognized-15986.czi has 3 z slices, so it is used here instead of
    // zeiss.czi. Its getRect() also reports a large, non-zero-based origin (see the comment
    // above), and its level 0 is far too large for a whole-level read, so a small sub-rect at
    // the coarsest level is used in both the level path and the scene path.
    {
        std::string zStackFilePath = TestTools::getFullTestImagePath("czi", "30-10-2020_NothingRecognized-15986.czi");
        std::shared_ptr<slideio::CVSlide> zStackSlide = driver.openFile(zStackFilePath);
        ASSERT_TRUE(zStackSlide != nullptr);
        std::shared_ptr<slideio::CVScene> zStackScene = zStackSlide->getScene(0);
        ASSERT_TRUE(zStackScene != nullptr);
        ASSERT_LT(1, zStackScene->getNumZSlices());

        const int zNumLevels = zStackScene->getNumZoomLevels();
        const int coarsestLevel = zNumLevels - 1;
        const slideio::LevelInfo* info = zStackScene->getZoomLevelInfo(coarsestLevel);
        const double coarsestScale = info->getScale();
        const cv::Size blockSize(256, 256);
        // The level-space rect this level read asks for...
        const cv::Rect levelRect(cv::Point(0, 0), blockSize);
        // ...and the zero-based scene-space rect that covers the same area at full
        // resolution, so the scene path resamples down to the same content.
        const cv::Size sceneRectSize(static_cast<int>(std::lround(blockSize.width / coarsestScale)),
                                     static_cast<int>(std::lround(blockSize.height / coarsestScale)));
        const cv::Rect sceneRect(cv::Point(0, 0), sceneRectSize);
        cv::Mat viaLevel, viaScene;
        zStackScene->readResampledLevel4DBlockChannels(coarsestLevel, levelRect, blockSize, {},
                                                       cv::Range(1, 2), cv::Range(0, 1), viaLevel);
        zStackScene->readResampled4DBlockChannels(sceneRect, blockSize, {},
                                                  cv::Range(1, 2), cv::Range(0, 1), viaScene);
        ASSERT_EQ(viaScene.size(), viaLevel.size());
        EXPECT_LE(0.95, slideio::ImageTools::computeSimilarity2(viaLevel, viaScene));
    }
}

// The generic CVScene default clamps levelRect to the level and then re-derives a zoom
// index from the requested output size; when that re-derived index happens to agree with
// the level actually asked for, the default's output is byte-identical to a direct read of
// that other level. So reading a level-0 sub-rect resampled down to level 1's local scale
// must NOT come back identical to a native read of the corresponding level-1 sub-rect --
// equality would mean level 0's read was actually served by level 1.
//
// Uses small sub-rectangles rather than whole levels: level 0 of this fixture is
// 49132x48722, and a whole-level read at that size is prohibitively expensive here. Levels 0
// and 1 (rather than the coarsest pair) are used because they are the most collapse-prone --
// exactly the pair the reference tests for other drivers exercise.
TEST(CZIImageDriver, readLevelDoesNotReuseAdjacentLevel)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }
    std::string filePath = TestTools::getFullTestImagePath("czi", "30-10-2020_NothingRecognized-15986.czi");
    slideio::CZIImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);
    ASSERT_LE(2, scene->getNumZoomLevels());

    const slideio::LevelInfo* level0 = scene->getZoomLevelInfo(0);
    const slideio::LevelInfo* level1 = scene->getZoomLevelInfo(1);
    ASSERT_TRUE(level0 != nullptr);
    ASSERT_TRUE(level1 != nullptr);
    const double scale = level1->getScale() / level0->getScale();

    // A rect well inside both levels' bounds; level 0's rect scaled down by the level 0->1
    // ratio gives the corresponding level-1 rect.
    const cv::Rect rect0(20000, 20000, 512, 512);
    const cv::Size blockSize(256, 256);
    const cv::Rect rect1(static_cast<int>(rect0.x * scale), static_cast<int>(rect0.y * scale),
                          blockSize.width, blockSize.height);

    // Read the level-0 sub-rect resampled to level 1's local scale -- must be served from
    // level 0 directly, not silently reselected to level 1 by the generic base default.
    cv::Mat viaLevel0Resampled;
    scene->readResampledLevelBlockChannels(0, rect0, blockSize, {}, viaLevel0Resampled);
    // Read the corresponding level-1 sub-rect natively: no resampling at all.
    cv::Mat viaLevel1Native;
    scene->readResampledLevelBlockChannels(1, rect1, blockSize, {}, viaLevel1Native);

    ASSERT_EQ(blockSize, viaLevel0Resampled.size());
    ASSERT_EQ(blockSize, viaLevel1Native.size());
    // The two are independently encoded streams; equality means level 0's read was actually
    // served from level 1.
    EXPECT_GT(cv::norm(viaLevel0Resampled, viaLevel1Native, cv::NORM_INF), 0);
}