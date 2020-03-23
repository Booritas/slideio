// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/core/imagedrivermanager.hpp"
#include "slideio/drivers/czi/cziimagedriver.hpp"
#include "slideio/drivers/czi/czislide.hpp"
#include "testtools.hpp"
#include <opencv2/imgcodecs.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include "slideio/scene.hpp"

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
        scene->readResampled4DBlockChannels(sceneRect,sceneRect.size(), channelIndices,
            {2,5}, {0,1}, raster);
        ASSERT_EQ(raster.channels(), zSliceRange.size());
        for(int zSliceIndex=zSliceRange.start; zSliceIndex<zSliceRange.end; ++zSliceIndex)
        {
            cv::Mat sliceRaster;
            cv::extractChannel(raster, sliceRaster, zSliceIndex-zSliceRange.start);
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

//TEST(CZIImageDriver, openFile1)
//{
//    slideio::CZIImageDriver driver;
//    std::string filePath = R"del(c:\Images\CZI\BPAE-Cells_63x_oversampled-3chZ(WF).czi)del";
//    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
//    ASSERT_TRUE(slide!=nullptr);
//}

////TODO: CLEAR COMMENTED OUT TESTS
//#include <opencv2/imgproc.hpp>
//#include <opencv2/highgui.hpp>
//
//TEST(CZIImageDriver, readBlock3)
//{
//    slideio::CZIImageDriver driver;
//    std::string filePath = TestTools::getTestImagePath("czi","test2.czi");
//    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
//    ASSERT_TRUE(slide!=nullptr);
//    int numScenes = slide->getNumScenes();
//    auto scene = slide->getScene(2);
//    ASSERT_FALSE(scene == nullptr);
//    auto sceneRect = scene->getRect();
//    sceneRect.x = 0;
//    sceneRect.y = 0;
//    cv::Mat raster;
//    std::vector<int> channelIndices;
//    cv::Size size = {550, 345};
//    scene->readResampledBlockChannels(sceneRect, size, channelIndices, raster);
//    cv::namedWindow( "Display window", cv::WINDOW_AUTOSIZE );
//    cv::imshow( "Display window", raster);
//    cv::waitKey(0);
//}

//TEST(CZIImageDriver, readBlock4)
//{
//    slideio::CZIImageDriver driver;
//    std::string filePath = TestTools::getTestImagePath("czi","test.czi");
//    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
//    ASSERT_TRUE(slide!=nullptr);
//    int numScenes = slide->getNumScenes();
//    auto scene = slide->getScene(0);
//    ASSERT_FALSE(scene == nullptr);
//    auto sceneRect = scene->getRect();
//    cv::Mat raster;
//    std::vector<int> channelIndices = {0};
//    cv::Size size = sceneRect.size();
//    size.width/=4;
//    size.height/=4;
//    scene->readResampledBlockChannels(sceneRect, size, channelIndices, raster);
//    double dmax, dmin;
//    cv::minMaxLoc(raster, &dmin, &dmax);
//    cv::Mat dst;
//    raster.convertTo(dst,CV_8U,255./dmax, 0);
//    namedWindow( "Display window", WINDOW_AUTOSIZE );
//    imshow( "Display window", dst);
//    waitKey(0);
//}
//
//TEST(CZIImageDriver, readBlock5)
//{
//    slideio::CZIImageDriver driver;
//    std::string filePath = TestTools::getTestImagePath("czi","test2.czi");
//    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
//    ASSERT_TRUE(slide!=nullptr);
//    int numScenes = slide->getNumScenes();
//    cv::Rect sceneRect;
//    for(int sceneIndex=0; sceneIndex<numScenes; ++sceneIndex)
//    {
//        auto scene = slide->getScene(sceneIndex);
//        sceneRect = scene->getRect();
//    }
//    auto scene = slide->getScene(3);
//    sceneRect = scene->getRect();
//    ASSERT_FALSE(scene == nullptr);
//    const int sx = sceneRect.width/2;
//    const int sy = sceneRect.height/2;
//    const double cof = 0.2;//512./std::max(sx,sy);
//    const int bsx = static_cast<int>(std::round(static_cast<double>(sx) * cof));
//    const int bsy = static_cast<int>(std::round(static_cast<double>(sy) * cof));
//    const cv::Size blockSize = {bsx, bsy};
//    double zx = double(bsx)/double(sx);
//    double yx = double(bsy)/double(sy);
//    const int centerX = sceneRect.width/2;
//    const int centerY = sceneRect.height/2;
//    cv::Rect blockRect = {1000, 1000, sx,sy};
//    cv::Mat raster;
//    scene->readResampledBlock(blockRect, blockSize, raster);
//    double dmax, dmin;
//    cv::minMaxLoc(raster, &dmin, &dmax);
//    cv::Mat dst;
//    raster.convertTo(dst,CV_8U,255./dmax, 0);
//    namedWindow( "Display window", WINDOW_AUTOSIZE );
//    imshow( "Display window", dst);
//    waitKey(0);
//}
//TEST(CZIImageDriver, readBlock2)
//{
//    slideio::CZIImageDriver driver;
//    std::string filePath = TestTools::getTestImagePath("czi","03_15_2019_DSGN0549_C_fov_9_633.czi");
//    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
//    ASSERT_TRUE(slide!=nullptr);
//    int numScenes = slide->getNumScenes();
//    auto scene = slide->getScene(0);
//    ASSERT_FALSE(scene == nullptr);
//    auto sceneRect = scene->getRect();
//    cv::Mat raster;
//    std::vector<int> channelIndices = {0,1,2};
//    cv::Size size = sceneRect.size();
//    size.width/=3;
//    size.height/=3;
//    scene->readResampledBlockChannels(sceneRect, size, channelIndices, raster);
//    namedWindow( "Display window", WINDOW_AUTOSIZE );
//    imshow( "Display window", raster);
//    waitKey(0);
//}
