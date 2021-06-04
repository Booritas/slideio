// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>


#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>


#include "slideio/core/imagedrivermanager.hpp"
#include "testtools.hpp"
#include "slideio/scene.hpp"
#include "slideio/core/cvtools.hpp"
#include "slideio/drivers/dcm/dcmimagedriver.hpp"
#include "slideio/imagetools/imagetools.hpp"

using namespace slideio;

TEST(DCMImageDriver, DriverManager_getDriverIDs)
{
    std::vector<std::string> driverIds = ImageDriverManager::getDriverIDs();
    auto it = std::find(driverIds.begin(), driverIds.end(), "DCM");
    EXPECT_FALSE(it == driverIds.end());
}

TEST(DCMImageDriver, getID)
{
    DCMImageDriver driver;
    std::string id = driver.getID();
    EXPECT_EQ(id, "DCM");
}

TEST(DCMImageDriver, canOpenFile)
{
    DCMImageDriver driver;
    EXPECT_TRUE(driver.canOpenFile("c:\\abbb\\a.dcm"));
    EXPECT_FALSE(driver.canOpenFile("c:\\abbb\\a.scn.tmp"));
}

TEST(DCMImageDriver, openFile)
{
    DCMImageDriver driver;
    std::string slidePath = TestTools::getTestImagePath(
        "dcm", "benigns_01/patient0186/0186.LEFT_CC.dcm");
    auto slide = driver.openFile(slidePath);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene);
    const cv::Rect rect = scene->getRect();
    const cv::Rect refRect = {0, 0, 3984, 5528};
    EXPECT_EQ(rect, refRect);
    const int numChannels = scene->getNumChannels();
    const int numSlices = scene->getNumZSlices();
    const int numFrames = scene->getNumTFrames();
    EXPECT_EQ(numChannels, 1);
    EXPECT_EQ(numSlices, 1);
    EXPECT_EQ(numFrames, 1);
    EXPECT_EQ(scene->getName(), "case0377");
    const Compression cmp = scene->getCompression();
    EXPECT_EQ(cmp, Compression::Jpeg);
}


TEST(DCMImageDriver, openDirectory)
{
    if (!TestTools::isPrivateTestEnabled())
    {
        GTEST_SKIP() <<
            "Skip private test because private dataset is not enabled";
    }
    DCMImageDriver driver;
    std::string slidePath = TestTools::getTestImagePath(
        "dcm", "series/series_1", true);
    auto slide = driver.openFile(slidePath);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene);
    const cv::Rect rect = scene->getRect();
    const cv::Rect refRect = {0, 0, 512, 512};
    EXPECT_EQ(rect, refRect);
    const int numChannels = scene->getNumChannels();
    const int numSlices = scene->getNumZSlices();
    const int numFrames = scene->getNumTFrames();
    EXPECT_EQ(numChannels, 1);
    EXPECT_EQ(numSlices, 15);
    EXPECT_EQ(numFrames, 1);
    EXPECT_EQ(scene->getName(), "COU IV");
}


TEST(DCMImageDriver, openDirectoryRecursively)
{
    if (!TestTools::isPrivateTestEnabled())
    {
        GTEST_SKIP() <<
            "Skip private test because private dataset is not enabled";
    }
    DCMImageDriver driver;
    std::string slidePath = TestTools::getTestImagePath("dcm", "series", true);
    auto slide = driver.openFile(slidePath);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 2);
    auto scene = slide->getScene(0);
    const std::string sceneName = scene->getName();
    if (sceneName == "COU IV")
    {
        scene = slide->getScene(1);
    }
    ASSERT_TRUE(scene);
    const cv::Rect rect = scene->getRect();
    const cv::Rect refRect = {0, 0, 512, 512};
    EXPECT_EQ(rect, refRect);
    const int numChannels = scene->getNumChannels();
    const int numSlices = scene->getNumZSlices();
    const int numFrames = scene->getNumTFrames();
    EXPECT_EQ(numChannels, 1);
    EXPECT_EQ(numSlices, 9);
    EXPECT_EQ(numFrames, 1);
    EXPECT_EQ(scene->getName(), "1.2.276.0.7230010.3.100.1.1");
}

TEST(DCMImageDriver, readSimpleFileWholeBlock)
{
    std::string slidePath = TestTools::getTestImagePath(
        "dcm", "bare.dev/OT-MONO2-8-hip.dcm");
    std::string testPath = TestTools::getTestImagePath(
        "dcm", "bare.dev/OT-MONO2-8-hip.frames/frame0.png");

    DCMImageDriver driver;
    auto slide = driver.openFile(slidePath);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene);
    const cv::Rect rect = scene->getRect();
    cv::Mat image;
    scene->readBlock(rect, image);
    ASSERT_FALSE(image.empty());
    cv::Mat bmpImage;
    slideio::ImageTools::readGDALImage(testPath, bmpImage);
    cv::Mat bmpBlock = bmpImage(rect);
    double similarity = ImageTools::computeSimilarity(image, bmpImage);
    EXPECT_LT(0.99, similarity);
}

TEST(DCMImageDriver, readSimpleFileResampled)
{
    std::string slidePath = TestTools::getTestImagePath(
        "dcm", "bare.dev/OT-MONO2-8-hip.dcm");
    std::string testPath = TestTools::getTestImagePath(
        "dcm", "bare.dev/OT-MONO2-8-hip.frames/frame0.png");

    DCMImageDriver driver;
    auto slide = driver.openFile(slidePath);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene);
    const cv::Rect rect = {100, 100, 400, 400};
    const cv::Size size = {200, 200};
    cv::Mat image;
    scene->readResampledBlock(rect, size, image);
    ASSERT_FALSE(image.empty());
    cv::Mat bmpImage;
    slideio::ImageTools::readGDALImage(testPath, bmpImage);
    cv::Mat bmpBlock = bmpImage(rect);
    cv::Mat resizedBlock;
    cv::resize(bmpBlock, resizedBlock, size);
    double similarity = ImageTools::computeSimilarity(image, resizedBlock);
    EXPECT_LT(0.99, similarity);
}

TEST(DCMImageDriver, readSingleFrame)
{
    std::string slidePath = TestTools::getTestImagePath("dcm", "benigns_01/patient0186/0186.LEFT_MLO.dcm");
    std::string testPath = TestTools::getTestImagePath("dcm", "benigns_01/patient0186/0186.LEFT_MLO.frames/frame0.tif");

    DCMImageDriver driver;
    auto slide = driver.openFile(slidePath);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene);
    cv::Mat image;
    cv::Rect rect = scene->getRect();
    cv::Size size = rect.size();
    slideio::DataType dt = scene->getChannelDataType(0);
    scene->readResampledBlock(rect, size, image);
    ASSERT_FALSE(image.empty());
    image.convertTo(image, CV_MAKE_TYPE(CV_8U, 1));
    cv::Mat testImage;
    slideio::ImageTools::readGDALImage(testPath, testImage);
    double similarity = ImageTools::computeSimilarity(image, testImage);
    EXPECT_LT(0.99, similarity);
}

TEST(DCMImageDriver, readSingleFrameROIRescale)
{
    std::string slidePath = TestTools::getTestImagePath("dcm", "benigns_01/patient0186/0186.LEFT_MLO.dcm");
    std::string testPath = TestTools::getTestImagePath("dcm", "benigns_01/patient0186/0186.LEFT_MLO.frames/frame0.tif");

    DCMImageDriver driver;
    auto slide = driver.openFile(slidePath);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene);
    cv::Mat image;
    cv::Rect rect = { 1000, 500, 600, 1000 };
    cv::Size size = { 300, 500 };
    slideio::DataType dt = scene->getChannelDataType(0);
    scene->readResampledBlock(rect, size, image);
    ASSERT_FALSE(image.empty());
    ASSERT_EQ(300, image.size().width);
    ASSERT_EQ(500, image.size().height);
    image.convertTo(image, CV_MAKE_TYPE(CV_8U, 1));
    cv::Mat testImage;
    slideio::ImageTools::readGDALImage(testPath, testImage);
    cv::Mat roi = testImage(rect);
    cv::resize(roi, roi, size);
    double similarity = ImageTools::computeSimilarity(image, roi);
    EXPECT_LT(0.99, similarity);
}

TEST(DCMImageDriver, readMultiFrameROIRescale)
{
    std::string slidePath = TestTools::getTestImagePath("dcm", "bare.dev/XA-MONO2-8-12x-catheter");
    std::string testPath1 = TestTools::getTestImagePath("dcm", "bare.dev/XA-MONO2-8-12x-catheter.frames/frame5.png");
    std::string testPath2 = TestTools::getTestImagePath("dcm", "bare.dev/XA-MONO2-8-12x-catheter.frames/frame6.png");

    DCMImageDriver driver;
    auto slide = driver.openFile(slidePath);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene);
    cv::Mat image;
    cv::Rect rect = { 100, 50, 300, 360 };
    cv::Size size = { 150, 180 };
    scene->readResampled4DBlock(rect, size, cv::Range(5,7), cv::Range(0,1),image);
    ASSERT_FALSE(image.empty());
    ASSERT_EQ(150, image.size[1]);
    ASSERT_EQ(180, image.size[0]);
    ASSERT_EQ(2, image.size[2]);
    cv::Mat slice5, slice6;
    CVTools::extractSliceFrom3D(image, 0, slice5);
    CVTools::extractSliceFrom3D(image, 1, slice6);

    cv::Mat testImage5;
    slideio::ImageTools::readGDALImage(testPath1, testImage5);
    cv::Mat roi5 = testImage5(rect);
    cv::resize(roi5, roi5, size);

    cv::Mat testImage6;
    slideio::ImageTools::readGDALImage(testPath2, testImage6);
    cv::Mat roi6 = testImage6(rect);
    cv::resize(roi6, roi6, size);
    double similarity = ImageTools::computeSimilarity(slice5, roi5);
    EXPECT_LT(0.999, similarity);
    similarity = ImageTools::computeSimilarity(slice6, roi6);
    EXPECT_LT(0.999, similarity);
}

TEST(DCMImageDriver, readDirectory3D)
{
    if (!TestTools::isPrivateTestEnabled())
    {
        GTEST_SKIP() <<
            "Skip private test because private dataset is not enabled";
    }
    DCMImageDriver driver;
    std::string slidePath = TestTools::getTestImagePath("dcm", "series/series_1", true);
    std::string testImagePath = TestTools::getTestImagePath("dcm", "series/series_1/tests/IMG-0001-00005.tiff", true);
    auto slide = driver.openFile(slidePath);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene);
    const cv::Rect rect = scene->getRect();
    cv::Mat image;
    const int slices = 9;
    scene->read4DBlock(rect, cv::Range(0, slices), cv::Range(0, 1), image);
    ASSERT_FALSE(image.empty());
    EXPECT_EQ(image.dims, 3);
    EXPECT_EQ(image.size[0], rect.width);
    EXPECT_EQ(image.size[1], rect.height);
    EXPECT_EQ(image.size[2], slices);
    EXPECT_EQ(image.channels(), 1);
    cv::Mat sliceRaster;
    CVTools::extractSliceFrom3D(image, 4, sliceRaster);

    cv::Mat bmpImage;
    ImageTools::readGDALImage(testImagePath, bmpImage);
    double similarity = ImageTools::computeSimilarity(sliceRaster, bmpImage);
    EXPECT_EQ(1, similarity);
}

TEST(DCMImageDriver, openDicomDirFile)
{
    DCMImageDriver driver;
    std::string slidePath = TestTools::getFullTestImagePath("dcm", "spine_mr/DICOMDIR");
    auto slide = driver.openFile(slidePath);
    const int numScenes = slide->getNumScenes();
    EXPECT_EQ(16, numScenes);
    struct SceneInfo
    {
        cv::Size size;
        int numFrames;
        int numChannels;
    };
    SceneInfo scenes[] = 
    {
        {
            cv::Size(256,256),
            9,
            1
        },
        {
            cv::Size(512,512),
            13,
            1
        },
        {
            cv::Size(64,64),
            118,
            1
        },
    };
    const int numTestScenes = sizeof(scenes) / sizeof(scenes[0]);
    for(int sceneIndex=0; sceneIndex< numTestScenes; ++sceneIndex)
    {
        auto scene = slide->getScene(sceneIndex);
        cv::Size size = scene->getRect().size();
        int numFrames = scene->getNumZSlices();
        int numChannels = scene->getNumChannels();
        EXPECT_EQ(size, scenes[sceneIndex].size);
        EXPECT_EQ(numFrames, scenes[sceneIndex].numFrames);
        EXPECT_EQ(numChannels, scenes[sceneIndex].numChannels);
    }
    auto scene = slide->getScene(1);
    cv::Rect rect = { 100, 150, 200, 300 };
    cv::Mat image;
    scene->read4DBlock(rect, cv::Range(3, 7), cv::Range(0, 1), image);
    ASSERT_FALSE(image.empty());
    EXPECT_EQ(300, image.size[0]);
    EXPECT_EQ(200, image.size[1]);
    EXPECT_EQ(4, image.size[2]);

}
