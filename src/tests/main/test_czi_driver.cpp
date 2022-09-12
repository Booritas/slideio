// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/slideio/imagedrivermanager.hpp"
#include "slideio/drivers/czi/cziimagedriver.hpp"
#include "slideio/drivers/czi/czislide.hpp"
#include "tests/testlib/testtools.hpp"
#include <opencv2/imgcodecs.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include "slideio/slideio/scene.hpp"
#include "slideio/core/cvtools.hpp"
#include "slideio/imagetools/imagetools.hpp"

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
        cv::Mat bmpImage = cv::imread(channelBmps[channelIndex], cv::IMREAD_GRAYSCALE);
        // compare equality of rasters from bmp and czi file
        int compare = std::memcmp(raster.data, bmpImage.data, raster.total()*raster.elemSize());
        EXPECT_EQ(compare, 0);
    }
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
            cv::Mat bmpImage = cv::imread(bmpFilePath, cv::IMREAD_GRAYSCALE);
            int compare = std::memcmp(sliceRaster.data, bmpImage.data, sliceRaster.total()*sliceRaster.elemSize());
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
        EXPECT_TRUE(boost::algorithm::starts_with(metadata, header));
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
    EXPECT_THROW(driver.openFile(filePath),std::runtime_error);
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
    double score = slideio::ImageTools::computeSimilarity(auxRaster, testRaster);
    ASSERT_DOUBLE_EQ(score, 1.);
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

//TODO: CLEAR COMMENTED OUT TESTS
//#include <opencv2/imgproc.hpp>
//#include <opencv2/highgui.hpp>

//TEST(CZIImageDriver, readBlock3)
//{
//    slideio::CZIImageDriver driver;
//    std::string filePath(R"(d:\Projects\slideio\slideio_extra_priv\testdata\cv\slideio\czi\jxr-rgb-5scenes.czi)");
//    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
//    ASSERT_TRUE(slide!=nullptr);
//    int numScenes = slide->getNumScenes();
//    auto scene = slide->getScene(2);
//    ASSERT_FALSE(scene == nullptr);
//    cv::Rect sceneRect = scene->getRect();
//    double coef = 400. / sceneRect.width;
//    int height = (int)std::round(coef * sceneRect.height);
//    cv::Size size = { 400, height };
//    cv::Mat raster;
//    sceneRect.x = sceneRect.y = 0;
//    scene->readResampledBlock(sceneRect, size, raster);
//    //cv::namedWindow( "Display window", cv::WINDOW_AUTOSIZE );
//    //cv::imshow( "Display window", raster);
//    //cv::waitKey(0);
//    int a = 0;
//}
