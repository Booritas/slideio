﻿// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/slideio/imagedrivermanager.hpp"
#include "slideio/drivers/scn/scnimagedriver.hpp"
#include "testtools.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/drivers/scn/scnscene.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "slideio/imagetools/imagetools.hpp"


TEST(SCNImageDriver, DriverManager_getDriverIDs)
{
    std::vector<std::string> driverIds = slideio::ImageDriverManager::getDriverIDs();
    auto it = std::find(driverIds.begin(),driverIds.end(), "SCN");
    EXPECT_FALSE(it==driverIds.end());
}
TEST(SCNImageDriver, getID)
{
    slideio::SCNImageDriver driver;
    std::string id = driver.getID();
    EXPECT_EQ(id,"SCN");
}

TEST(SCNImageDriver, canOpenFile)
{
    slideio::SCNImageDriver driver;
    EXPECT_TRUE(driver.canOpenFile("c:\\abbb\\a.scn"));
    EXPECT_FALSE(driver.canOpenFile("c:\\abbb\\a.scn.tmp"));
}

TEST(SCNImageDriver, slideRawMetadata)
{
    const std::string images[] = {
        "Leica-Fluorescence-1.scn"
    };
    slideio::SCNImageDriver driver;
    for (const auto& imageName : images)
    {
        std::string filePath = TestTools::getTestImagePath("scn", imageName);
        std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
        const std::string& metadata = slide->getRawMetadata();
        EXPECT_GT(metadata.length(), 0);
        const std::string header("<?xml version=\"1.0\"?>");
        EXPECT_TRUE(boost::algorithm::starts_with(metadata, header));
    }
}


TEST(SCNImageDriver, openFile)
{
    struct SceneInfo
    {
        std::string name;
        cv::Rect rect;
        int numChannels;
        double magnification;
        double res_x;
        double res_y;
        slideio::DataType dataType;
    };
    const SceneInfo infos[] =
    {
        {"image_0000000586", {0, 0, 1616, 4668}, 3, 0.60833, 0.16438446e-4, 0.16438446e-4, slideio::DataType::DT_Byte},
        {"image_0000000590", {0, 0, 1616, 4668}, 3, 0.60833, 0.16438446e-4, 0.16438446e-4, slideio::DataType::DT_Byte},
        {"image_0000000591", {16306, 40361, 4737, 6338}, 3, 20., 0.5e-6, 0.5e-6, slideio::DataType::DT_Byte}
    };
    slideio::SCNImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("scn","Leica-Fluorescence-1.scn");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide!=nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    std::string channelNames[] = { "405|Empty", "L5|Empty", "TX2|Empty"};
    for(int sceneIndex=0; sceneIndex<3; ++sceneIndex)
    {
        const SceneInfo& sceneInfo = infos[sceneIndex];
        std::shared_ptr<slideio::CVScene> scene;
        if (sceneIndex == 0) {
            scene = slide->getAuxImage("Macro");
        }
        else if (sceneIndex == 1) {
            scene = slide->getAuxImage("Macro~1");
        }
        else {
            scene = slide->getScene(0);
        }
        ASSERT_FALSE(scene == nullptr);
        EXPECT_EQ(scene->getName(), sceneInfo.name);
        EXPECT_EQ(scene->getRect(), sceneInfo.rect);
        EXPECT_EQ(scene->getNumChannels(), sceneInfo.numChannels);
        EXPECT_EQ(scene->getMagnification(), sceneInfo.magnification);
        slideio::Resolution res = scene->getResolution();
        EXPECT_NEAR(res.x, sceneInfo.res_x, sceneInfo.res_x*0.00001);
        EXPECT_NEAR(res.y, sceneInfo.res_y, sceneInfo.res_y*0.00001);
        for(int channelIndex=0; channelIndex<sceneInfo.numChannels; ++channelIndex)
        {
            EXPECT_EQ((int)scene->getChannelDataType(channelIndex), (int)sceneInfo.dataType);
            if(sceneIndex==2)
            {
                EXPECT_EQ(scene->getChannelName(channelIndex), channelNames[channelIndex]);
            }
            else
            {
                EXPECT_EQ(scene->getChannelName(channelIndex), "");
            }
        }
    }
}

TEST(SCNImageDriver, getChannelDir)
{
    slideio::SCNImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("scn", "Leica-Fluorescence-1.scn");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    {
        std::shared_ptr<slideio::SCNScene> scene = std::dynamic_pointer_cast<slideio::SCNScene>(slide->getAuxImage("Macro"));
        ASSERT_FALSE(scene == nullptr);
        auto dirs = scene->getChannelDirectories(0);
        EXPECT_EQ(dirs[0].width, 1616);
        EXPECT_EQ(dirs[0].height, 4668);
        int prevWidth = -1;
        for(auto & dir: dirs)
        {
            if(prevWidth > 0)
            {
                EXPECT_LT(dir.width, prevWidth);
            }
            prevWidth = dir.width;
        }
    }
    {
        std::shared_ptr<slideio::SCNScene> scene = std::dynamic_pointer_cast<slideio::SCNScene>(slide->getScene(0));
        ASSERT_FALSE(scene == nullptr);
        auto dirs = scene->getChannelDirectories(0);
        EXPECT_EQ(dirs[0].width, 4737);
        EXPECT_EQ(dirs[0].height, 6338);
        int prevWidth = -1;
        for (auto& dir : dirs)
        {
            if (prevWidth > 0)
            {
                EXPECT_LT(dir.width, prevWidth);
            }
            prevWidth = dir.width;
        }
    }
}

TEST(SCNImageDriver, findZoomDirectory)
{
    slideio::SCNImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("scn", "Leica-Fluorescence-1.scn");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    {
        std::shared_ptr<slideio::SCNScene> scene = std::dynamic_pointer_cast<slideio::SCNScene>(slide->getAuxImage("Macro"));
        ASSERT_FALSE(scene == nullptr);
        {
            const auto& dir = scene->findZoomDirectory(0, 1);
            EXPECT_EQ(dir.dirIndex, 0);
        }
        {
            const auto& dir = scene->findZoomDirectory(0, 0.5);
            EXPECT_EQ(dir.dirIndex, 0);
        }
        {
            const auto& dir = scene->findZoomDirectory(0, 0.25);
            EXPECT_EQ(dir.dirIndex, 1);
        }
        {
            const auto& dir = scene->findZoomDirectory(0, 0.01);
            EXPECT_EQ(dir.dirIndex, 2);
        }
    }
    {
        std::shared_ptr<slideio::SCNScene> scene = std::dynamic_pointer_cast<slideio::SCNScene>(slide->getScene(0));
        ASSERT_FALSE(scene == nullptr);
        {
            const auto& dir = scene->findZoomDirectory(2, 1);
            EXPECT_EQ(dir.dirIndex, 8);
        }
        {
            const auto& dir = scene->findZoomDirectory(2, 0.5);
            EXPECT_EQ(dir.dirIndex, 8);
        }
        {
            const auto& dir = scene->findZoomDirectory(2, 0.25);
            EXPECT_EQ(dir.dirIndex, 11);
        }
        {
            const auto& dir = scene->findZoomDirectory(2, 0.01);
            EXPECT_EQ(dir.dirIndex, 17);
        }
    }
}

TEST(SCNImageDriver, getTileCount)
{
    slideio::SCNImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("scn", "Leica-Fluorescence-1.scn");
    std::vector<slideio::TiffDirectory> dirs;
    slideio::TiffTools::scanFile(filePath, dirs);
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    {
        std::shared_ptr<slideio::SCNScene> scene =
            std::dynamic_pointer_cast<slideio::SCNScene>(slide->getAuxImage("Macro"));
        ASSERT_FALSE(scene == nullptr);
        SCNTilingInfo info;
        info.channel2ifd[0] = &dirs[0];
        int count = scene->getTileCount(&info);
        EXPECT_EQ(count, 40);
    }
    {
        std::shared_ptr<slideio::SCNScene> scene =
            std::dynamic_pointer_cast<slideio::SCNScene>(slide->getScene(0));
        ASSERT_FALSE(scene == nullptr);
        SCNTilingInfo info;
        info.channel2ifd[0] = &dirs[8];
        int count = scene->getTileCount(&info);
        EXPECT_EQ(count, 130);
    }
}

TEST(SCNImageDriver, getTileRect)
{
    slideio::SCNImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("scn", "Leica-Fluorescence-1.scn");
    std::vector<slideio::TiffDirectory> dirs;
    slideio::TiffTools::scanFile(filePath, dirs);
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    {
        std::shared_ptr<slideio::SCNScene> scene =
            std::dynamic_pointer_cast<slideio::SCNScene>(slide->getAuxImage("Macro"));
        ASSERT_FALSE(scene == nullptr);
        SCNTilingInfo info;
        info.channel2ifd[0] = &dirs[0];
        cv::Rect tileRect;
        scene->getTileRect(0, tileRect, &info);
        EXPECT_EQ(tileRect, cv::Rect(0,0,512,512));
        scene->getTileRect(39, tileRect, &info);
        EXPECT_EQ(tileRect, cv::Rect(1536, 4608, 512, 512));
    }
}

TEST(SCNImageDriver, readTile_1_channel)
{
    slideio::SCNImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("scn", "Leica-Fluorescence-1.scn");
    std::string tilePath = TestTools::getTestImagePath("scn", "Leica-Fluorescence-1/dir_8_tile_6-8.bmp");
    std::vector<slideio::TiffDirectory> dirs;
    slideio::TiffTools::scanFile(filePath, dirs);
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    std::shared_ptr<slideio::SCNScene> scene =
        std::dynamic_pointer_cast<slideio::SCNScene>(slide->getScene(0));
    ASSERT_FALSE(scene == nullptr);
    SCNTilingInfo info;
    info.channel2ifd[0] = &dirs[8];
    cv::Mat raster;
    const std::vector<int> channelIndices = { 0 };
    const int tileIndex = 6 + 8*10;
    scene->readTile(tileIndex, channelIndices, raster, &info);
    cv::Mat bmpImage = cv::imread(tilePath, cv::IMREAD_GRAYSCALE);
    int compare = std::memcmp(raster.data, bmpImage.data, raster.total() * raster.elemSize());
    EXPECT_EQ(compare, 0);
}

TEST(SCNImageDriver, readTile_2_channels)
{
    slideio::SCNImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("scn", "Leica-Fluorescence-1.scn");
    std::string tilePath1 = TestTools::getTestImagePath("scn", "Leica-Fluorescence-1/dir_6_tile_6-8.bmp");
    std::string tilePath2 = TestTools::getTestImagePath("scn", "Leica-Fluorescence-1/dir_8_tile_6-8.bmp");
    std::vector<slideio::TiffDirectory> dirs;
    slideio::TiffTools::scanFile(filePath, dirs);
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    std::shared_ptr<slideio::SCNScene> scene =
        std::dynamic_pointer_cast<slideio::SCNScene>(slide->getScene(0));
    ASSERT_FALSE(scene == nullptr);
    SCNTilingInfo info;
    info.channel2ifd[0] = &dirs[6];
    info.channel2ifd[1] = &dirs[8];

    cv::Mat raster;
    const std::vector<int> channelIndices = { 0, 1 };
    const int tileIndex = 6 + 8 * 10;
    scene->readTile(tileIndex, channelIndices, raster, &info);

    std::vector<cv::Mat> tileChannels(2);
    tileChannels[0] = cv::imread(tilePath1, cv::IMREAD_GRAYSCALE);
    tileChannels[1] = cv::imread(tilePath2, cv::IMREAD_GRAYSCALE);

    cv::Mat bmpImage;
    cv::merge(tileChannels, bmpImage);
    int compare = std::memcmp(raster.data, bmpImage.data, raster.total() * raster.elemSize());
    EXPECT_EQ(compare, 0);
}

#include <opencv2/highgui.hpp>

TEST(SCNImageDriver, readTile_interleaved_channels)
{
    slideio::SCNImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("scn", "Leica-Fluorescence-1.scn");
    std::vector<slideio::TiffDirectory> dirs;
    slideio::TiffTools::scanFile(filePath, dirs);
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    std::shared_ptr<slideio::SCNScene> scene =
        std::dynamic_pointer_cast<slideio::SCNScene>(slide->getAuxImage("Macro"));
    ASSERT_FALSE(scene == nullptr);
    SCNTilingInfo info;
    info.channel2ifd[0] = &dirs[0];
    cv::Mat raster;
    const std::vector<int> channelIndices = { 2, 1, 0 };
    const int tileIndex = 1 + 7 * 4;
    scene->readTile(tileIndex, channelIndices, raster, &info);

    std::string tilePath = TestTools::getTestImagePath("scn", "Leica-Fluorescence-1/dir_0_tile_1-7.png");
    cv::Mat bmpImage;
    slideio::ImageTools::readGDALImage(tilePath, bmpImage);

    cv::Rect roi{ 0,0,512,200 };
    cv::Mat dif;
    cv::absdiff(raster(roi), bmpImage(roi), dif);
    cv::Mat dif1, dif2, dif3;
    cv::extractChannel(dif, dif1, 0);
    cv::extractChannel(dif, dif2, 1);
    cv::extractChannel(dif, dif3, 2);
    EXPECT_EQ(cv::countNonZero(dif1), 0);
    EXPECT_EQ(cv::countNonZero(dif2), 0);
    EXPECT_EQ(cv::countNonZero(dif3), 0);
}

TEST(SCNImageDriver, readTile_readBlock)
{
    slideio::SCNImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("scn", "Leica-Fluorescence-1.scn");
    std::string regionPath = TestTools::getTestImagePath("scn", "Leica-Fluorescence-1/x2500-y2338-600x500.bmp");
    cv::Mat region = cv::imread(regionPath, cv::IMREAD_COLOR);

    std::vector<slideio::TiffDirectory> dirs;
    slideio::TiffTools::scanFile(filePath, dirs);
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    std::shared_ptr<slideio::SCNScene> scene =
        std::dynamic_pointer_cast<slideio::SCNScene>(slide->getScene(0));
    ASSERT_FALSE(scene == nullptr);
    cv::Mat block;
    std::vector<int> channelIndices = { 0, 1, 2 };
    scene->readBlockChannels(cv::Rect(2500, 2338,600,500), channelIndices, block);
    const int compare = std::memcmp(block.data, region.data, block.total() * block.elemSize());
    EXPECT_EQ(compare, 0);
}

TEST(SCNImageDriver, readTile_readBlockResampling)
{
    slideio::SCNImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("scn", "Leica-Fluorescence-1.scn");
    std::string regionPath = TestTools::getTestImagePath("scn", "Leica-Fluorescence-1/x2500-y2338-600x500.bmp");
    cv::Mat region = cv::imread(regionPath, cv::IMREAD_COLOR);
    const double coef = 1. / 3.;
    const int width = std::lround(region.cols * coef);
    const int height = std::lround(region.rows * coef);
    const cv::Size scaledSize(width, height);
    cv::Mat scaledRegion;
    cv::resize(region, scaledRegion, scaledSize, 0, 0, cv::INTER_NEAREST);

    std::vector<slideio::TiffDirectory> dirs;
    slideio::TiffTools::scanFile(filePath, dirs);
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    std::shared_ptr<slideio::SCNScene> scene =
        std::dynamic_pointer_cast<slideio::SCNScene>(slide->getScene(0));
    ASSERT_FALSE(scene == nullptr);
    cv::Mat block;
    std::vector<int> channelIndices = { 0, 1, 2 };
    scene->readResampledBlockChannels(cv::Rect(2500, 2338, 600, 500), scaledSize, channelIndices, block);

    cv::Mat score;
    cv::matchTemplate(scaledRegion, block, score, cv::TM_CCOEFF_NORMED);
    double minScore(0), maxScore(0);
    cv::minMaxLoc(score, &minScore, &maxScore);


    ASSERT_LT(0.8, minScore);
}

TEST(SCNImageDriver, readThumbnail)
{
    slideio::SCNImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("scn", "Leica-Fluorescence-1.scn");
    std::string thumbnailPath = TestTools::getTestImagePath("scn", "Leica-Fluorescence-1/thumbnail.png");
    cv::Mat thumbnail;
    slideio::ImageTools::readGDALImage(thumbnailPath, thumbnail);
    slideio::SCNImageDriver imageDriver;
    auto slide = imageDriver.openFile(filePath);
    auto scene = slide->getAuxImage("Macro");
    auto rect = scene->getRect();
    cv::Mat raster;
    std::vector<int> channels = { 0, 1, 2 };
    double cof = 300. / double(rect.width);
    cv::Size size = { (int)std::lround(rect.width*cof), (int)std::lround(rect.height*cof) };
    scene->readResampledBlockChannels(rect, size, channels, raster);
    const int compare = std::memcmp(raster.data, thumbnail.data, raster.total() * raster.elemSize());
    EXPECT_EQ(compare, 0);
}

TEST(SCNImageDriver, auxImages)
{
    slideio::SCNImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("scn", "Leica-Fluorescence-1.scn");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numImages = slide->getNumAuxImages();
    ASSERT_EQ(numImages, 2);
    std::list<std::string> imageNames = slide->getAuxImageNames();
    ASSERT_FALSE(std::find(imageNames.begin(), imageNames.end(), "Macro") == imageNames.end());
    ASSERT_FALSE(std::find(imageNames.begin(), imageNames.end(), "Macro~1") == imageNames.end());
}

TEST(SCNImageDriver, supplementalImage)
{
    slideio::SCNImageDriver driver;
    std::string filePath = TestTools::getFullTestImagePath("scn", "ultivue/Leica Aperio Versa 5 channel fluorescent image.scn");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 3);
    const int numImages = slide->getNumAuxImages();
    ASSERT_EQ(numImages, 1);
    std::list<std::string> imageNames = slide->getAuxImageNames();
    ASSERT_TRUE(std::find(imageNames.begin(), imageNames.end(), "label") != imageNames.end());
    auto scene = slide->getAuxImage("label");
    auto rect = scene->getRect();
    cv::Mat label;
    scene->readBlock(rect, label);
    std::string testFilePath = TestTools::getFullTestImagePath("scn", "ultivue/test/Leica Aperio Versa 5 channel fluorescent image-label.png");
    cv::Mat expectedLabel;
    slideio::ImageTools::readGDALImage(testFilePath, expectedLabel);
    cv::Mat dif;
    cv::absdiff(label, expectedLabel, dif);
    cv::Mat dif1, dif2, dif3;
    cv::extractChannel(dif, dif1, 0);
    cv::extractChannel(dif, dif2, 1);
    cv::extractChannel(dif, dif3, 2);
    EXPECT_EQ(cv::countNonZero(dif1), 0);
    EXPECT_EQ(cv::countNonZero(dif2), 0);
    EXPECT_EQ(cv::countNonZero(dif3), 0);
}
