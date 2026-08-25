// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>


//#include <opencv2/highgui.hpp>
//#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>


#include "slideio/core/tools/tools.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"
#include "tests/testlib/testtools.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include "slideio/drivers/dcm/dcmimagedriver.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/slideio/slideio.hpp"

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
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
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
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
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

TEST(DCMImageDriver, getSceneIndex)
{
    if (!TestTools::isPrivateTestEnabled()) {
        GTEST_SKIP() <<
            "Skip private test because private dataset is not enabled";
    }
    DCMImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("dcm", "series", true);
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    auto slide = slideio::openSlide(filePath, "AUTO");
    ASSERT_TRUE(slide);
    EXPECT_EQ("DCM", slide->getDriverId());
    const int numScenes = slide->getNumScenes();
    EXPECT_EQ(2, numScenes);
    for (int iScene = 0; iScene < numScenes; ++iScene) {
        std::shared_ptr<slideio::CVScene> scene = slide->getScene(iScene)->getCVScene();
        EXPECT_TRUE(scene.get() != nullptr);
        EXPECT_EQ(iScene, scene->getSceneIndex());
        EXPECT_EQ(filePath, scene->getFilePath());
		EXPECT_EQ("DCM", scene->getDriverId());
    }
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
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
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
        "dcm", "barre.dev/OT-MONO2-8-hip.dcm");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    std::string testPath = TestTools::getTestImagePath(
        "dcm", "barre.dev/OT-MONO2-8-hip.frames/frame0.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testPath);

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
    slideio::ImageTools::readSmallImageRaster(testPath, bmpImage);
    cv::Mat bmpBlock = bmpImage(rect);
    double similarity = ImageTools::computeSimilarity(image, bmpImage);
    EXPECT_LT(0.99, similarity);
}

TEST(DCMImageDriver, getRawMetadata)
{
    std::string slidePath = TestTools::getTestImagePath(
        "dcm", "barre.dev/OT-MONO2-8-hip.dcm");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);

    DCMImageDriver driver;
    auto slide = driver.openFile(slidePath);
    EXPECT_EQ(slide->getMetadataFormat(), slideio::MetadataFormat::None);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    auto scene = slide->getScene(0);
	EXPECT_EQ(scene->getMetadataFormat(), slideio::MetadataFormat::JSON);
    std::string metadata = scene->getRawMetadata();
    ASSERT_LT(2, metadata.length());
    EXPECT_EQ('{', metadata.front());
    EXPECT_EQ('}', metadata.back());
}


TEST(DCMImageDriver, readSimpleFileResampled)
{
    std::string slidePath = TestTools::getTestImagePath(
        "dcm", "barre.dev/OT-MONO2-8-hip.dcm");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    std::string testPath = TestTools::getTestImagePath(
        "dcm", "barre.dev/OT-MONO2-8-hip.frames/frame0.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testPath);

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
    slideio::ImageTools::readSmallImageRaster(testPath, bmpImage);
    cv::Mat bmpBlock = bmpImage(rect);
    cv::Mat resizedBlock;
    cv::resize(bmpBlock, resizedBlock, size);
    double similarity = ImageTools::computeSimilarity(image, resizedBlock);
    EXPECT_LT(0.99, similarity);
}

TEST(DCMImageDriver, readSingleFrame)
{
    std::string slidePath = TestTools::getTestImagePath("dcm", "benigns_01/patient0186/0186.LEFT_MLO.dcm");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    std::string testPath = TestTools::getTestImagePath("dcm", "benigns_01/patient0186/0186.LEFT_MLO.frames/frame0.tif");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testPath);

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
    slideio::ImageTools::readSmallImageRaster(testPath, testImage);
    double similarity = ImageTools::computeSimilarity(image, testImage);
    EXPECT_LT(0.99, similarity);
}

TEST(DCMImageDriver, readSingleFrameROIRescale)
{
    std::string slidePath = TestTools::getTestImagePath("dcm", "benigns_01/patient0186/0186.LEFT_MLO.dcm");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    std::string testPath = TestTools::getTestImagePath("dcm", "benigns_01/patient0186/0186.LEFT_MLO.frames/frame0.tif");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testPath);

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
    slideio::ImageTools::readSmallImageRaster(testPath, testImage);
    cv::Mat roi = testImage(rect);
    cv::resize(roi, roi, size);
    double similarity = ImageTools::computeSimilarity(image, roi);
    EXPECT_LT(0.99, similarity);
}

TEST(DCMImageDriver, readMultiFrameROIRescale)
{
    std::string slidePath = TestTools::getTestImagePath("dcm", "barre.dev/XA-MONO2-8-12x-catheter");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    std::string testPath1 = TestTools::getTestImagePath("dcm", "barre.dev/XA-MONO2-8-12x-catheter.frames/frame5.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testPath1);
    std::string testPath2 = TestTools::getTestImagePath("dcm", "barre.dev/XA-MONO2-8-12x-catheter.frames/frame6.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testPath2);

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
    slideio::ImageTools::readSmallImageRaster(testPath1, testImage5);
    cv::Mat roi5 = testImage5(rect);
    cv::resize(roi5, roi5, size);

    cv::Mat testImage6;
    slideio::ImageTools::readSmallImageRaster(testPath2, testImage6);
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
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    std::string testImagePath = TestTools::getTestImagePath("dcm", "series/series_1/tests/IMG-0001-00005.tiff", true);
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testImagePath);
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
    ImageTools::readSmallImageRaster(testImagePath, bmpImage);
    double similarity = ImageTools::computeSimilarity(sliceRaster, bmpImage);
    //TestTools::showRasters(sliceRaster, bmpImage);
    EXPECT_EQ(1, similarity);
}

TEST(DCMImageDriver, DICOMDirgetSceneIndex)
{
    if (!TestTools::isPrivateTestEnabled()) {
        GTEST_SKIP() <<
            "Skip private test because private dataset is not enabled";
    }
    DCMImageDriver driver;
    std::string filePath = TestTools::getFullTestImagePath("dcm", "spine_mr/DICOMDIR");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);
    const int numScenes = slide->getNumScenes();
    EXPECT_EQ(16, numScenes);
    for (int iScene = 0; iScene < numScenes; ++iScene) {
        std::shared_ptr<slideio::CVScene> scene = slide->getScene(iScene);
        EXPECT_TRUE(scene.get() != nullptr);
        EXPECT_EQ(iScene, scene->getSceneIndex());
        EXPECT_EQ(filePath, scene->getFilePath());
    }
}


TEST(DCMImageDriver, openDicomDirFile)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() <<
            "Skip the test because full dataset is not enabled";
    }
    DCMImageDriver driver;
    std::string slidePath = TestTools::getFullTestImagePath("dcm", "spine_mr/DICOMDIR");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
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

TEST(DCMImageDriver, readBlockChangingBits)
{
    std::string slidePath = TestTools::getTestImagePath("dcm", "barre.dev/US-PAL-8-10x-echo");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    DCMImageDriver driver;
    auto slide = driver.openFile(slidePath);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene);
    const cv::Rect rect = scene->getRect();
    const int slices = scene->getNumZSlices();
    cv::Mat image;
    scene->read4DBlock(rect, cv::Range(0, slices), cv::Range(0, 1), image);
}

TEST(DCMImageDriver, openFileUtf8Path)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() <<
                     "Skip private test because private dataset is not enabled";
    }
    DCMImageDriver driver;
    std::string slidePath = TestTools::getFullTestImagePath(
        "unicode", u8"тест/CT-MONO2-12-lomb-an2");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    auto slide = driver.openFile(slidePath);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene);
    const cv::Rect rect = scene->getRect();
    const cv::Rect refRect = { 0, 0, 512, 512 };
    EXPECT_EQ(rect, refRect);
    cv::Mat raster;
    scene->readBlock(rect, raster);
    EXPECT_EQ(raster.cols, rect.width);
    EXPECT_EQ(raster.rows, rect.height);
}

TEST(DCMImageDriver, openWSIDirectory)
{
    std::list<std::string> auxNames = { "LOCALIZER", "LABEL", "OVERVIEW" };

    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() <<
        "Skip private test because private dataset is not enabled";
    }
    DCMImageDriver driver;
    std::string slidePath = TestTools::getFullTestImagePath(
            "dcm", "private/H01EBB50P-24777");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    auto slide = driver.openFile(slidePath);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    const int numAuxImages = scene->getNumAuxImages();
    EXPECT_EQ(3,numAuxImages);
    std::list<std::string> names = scene->getAuxImageNames();
    for(const auto& name : names) {
        EXPECT_TRUE(std::find(auxNames.begin(), auxNames.end(), name) != auxNames.end());
    }
}

TEST(DCMImageDriver, openSingleFileWSI)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() <<
            "Skip private test because private dataset is not enabled";
    }
    DCMImageDriver driver;
    std::string slidePath = TestTools::getFullTestImagePath(
        "dcm", "private/wsi/M01FBC14P-589_level-0.dcm");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    auto slide = driver.openFile(slidePath);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    auto scene = slide->getScene(0);
    ASSERT_FALSE(!scene);
    EXPECT_EQ(0, scene->getMagnification());
    EXPECT_EQ(0., scene->getResolution().x);
    EXPECT_EQ(0., scene->getResolution().y);
    EXPECT_EQ(Compression::Jpeg, scene->getCompression());
    EXPECT_EQ(3, scene->getNumChannels());
    EXPECT_EQ(1, scene->getNumZSlices());
    EXPECT_EQ("1.2.826.0.1.3680043.10.559.7459853763397301473967910469110355067", scene->getName());
    EXPECT_EQ(0, scene->getRect().x);
    EXPECT_EQ(0, scene->getRect().y);
    EXPECT_EQ(82432, scene->getRect().width);
    EXPECT_EQ(103936, scene->getRect().height);
}

TEST(DCMImageDriver, readBlockSingleFileWSI)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() <<
            "Skip private test because private dataset is not enabled";
    }
    DCMImageDriver driver;
    std::string slidePath = TestTools::getFullTestImagePath(
        "dcm", "private/wsi/M01FBC14P-589_level-0.dcm");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    std::string testFilePath = TestTools::getFullTestImagePath(
        "dcm", "private/wsi/M01FBC14P-589_level-0.block.dcm");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath);
    auto slide = driver.openFile(slidePath);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    auto scene = slide->getScene(0);
    ASSERT_FALSE(!scene);
    cv::Rect rectScene = scene->getRect();
    int x = rectScene.x + rectScene.width / 3;
    int y = rectScene.y + rectScene.height / 3;
    const int width = 600;
    const int height = 400;
    cv::Rect rectBlock = { x, y, width, height };
    cv::Mat raster;
    scene->readBlock(rectBlock, raster);
    cv::Mat testRaster;
    //TestTools::writePNG(raster, testFilePath);
    TestTools::readPNG(testFilePath, testRaster);
    TestTools::compareRasters(raster, testRaster);
    //TestTools::showRaster(raster);
}

TEST(DCMImageDriver, WSISingleFileGetSceneIndex)
{
    if (!TestTools::isPrivateTestEnabled()) {
        GTEST_SKIP() <<
            "Skip private test because private dataset is not enabled";
    }
    DCMImageDriver driver;
    std::string filePath = TestTools::getFullTestImagePath(
        "dcm", "private/wsi/M01FBC14P-589_level-0.dcm");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);
    const int numScenes = slide->getNumScenes();
    EXPECT_EQ(1, numScenes);
    for (int iScene = 0; iScene < numScenes; ++iScene) {
        std::shared_ptr<slideio::CVScene> scene = slide->getScene(iScene);
        EXPECT_TRUE(scene.get() != nullptr);
        EXPECT_EQ(iScene, scene->getSceneIndex());
        EXPECT_EQ(filePath, scene->getFilePath());
		EXPECT_EQ("DCM", scene->getDriverId());
    }
}

TEST(DCMImageDriver, readBlockResampleSingleFileWSI)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() <<
            "Skip private test because private dataset is not enabled";
    }
    DCMImageDriver driver;
    std::string slidePath = TestTools::getFullTestImagePath(
        "dcm", "private/wsi/M01FBC14P-589_level-0.dcm");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    std::string testFilePath = TestTools::getFullTestImagePath(
        "dcm", "private/wsi/M01FBC14P-589_level-0.block.dcm");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath);
    auto slide = driver.openFile(slidePath);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    auto scene = slide->getScene(0);
    ASSERT_FALSE(!scene);
    cv::Rect rectScene = scene->getRect();
    int x = rectScene.x + rectScene.width / 3;
    int y = rectScene.y + rectScene.height / 3;
    const int width = 600;
    const int height = 400;
    cv::Rect rectBlock = { x, y, width, height };
    cv::Mat raster;
    cv::Size blockSize = { 300, 200 };
    scene->readResampledBlock(rectBlock, blockSize, raster);
    cv::Mat testRaster;
    //TestTools::writePNG(raster, testFilePath);
    TestTools::readPNG(testFilePath, testRaster);
    cv::resize(testRaster, testRaster, blockSize);
    double sim = ImageTools::computeSimilarity2(raster, testRaster);
    EXPECT_LE(0.99, sim);
    //TestTools::showRasters(testRaster, raster);
}

TEST(DCMImageDriver, readResampledBlockWSIDirectory)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() <<
            "Skip private test because private dataset is not enabled";
    }
    DCMImageDriver driver;
    std::string slidePath = TestTools::getFullTestImagePath("dcm", "private/H01EBB50P-24777");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    std::string testFilePath1 = TestTools::getFullTestImagePath("dcm", "private/H01EBB50P-24777.block-2.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath1);
    std::string testFilePath2 = TestTools::getFullTestImagePath("dcm", "private/H01EBB50P-24777.block-3.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath2);
    auto slide = driver.openFile(slidePath);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    cv::Rect rectScene = scene->getRect();
    int x = rectScene.x + rectScene.width / 3;
    int y = rectScene.y + rectScene.height / 3;
    const int width = 600;
    const int height = 400;
    cv::Rect rectBlock = { x, y, width, height };
    cv::Mat raster;
    cv::Size blockSize = { 300, 200 };
    scene->readResampledBlock(rectBlock, blockSize, raster);
    cv::Mat testRaster;
    //TestTools::writePNG(raster, testFilePath1);
    TestTools::readPNG(testFilePath1, testRaster);
    cv::resize(testRaster, testRaster, blockSize);
    double sim = ImageTools::computeSimilarity2(raster, testRaster);
    EXPECT_LE(0.99, sim);
    //TestTools::showRasters(testRaster,raster);

    rectBlock = rectScene;
    const int blockWidth = 600;
    const double cof = static_cast<double>(blockWidth) / static_cast<double>(rectScene.width);
    const int blockHeigt = std::lround(cof * static_cast<double>(rectScene.height));
    blockSize = { blockWidth, blockHeigt };

    scene->readResampledBlock(rectBlock, blockSize, raster);
    //TestTools::writePNG(raster, testFilePath2);
    TestTools::readPNG(testFilePath2, testRaster);
    cv::resize(testRaster, testRaster, blockSize);
    sim = ImageTools::computeSimilarity2(raster, testRaster);
    EXPECT_LE(0.99, sim);
    //TestTools::showRasters(testRaster,raster);
}

TEST(DCMImageDriver, WSIDirGetSceneIndex)
{
    if (!TestTools::isPrivateTestEnabled()) {
        GTEST_SKIP() <<
            "Skip private test because private dataset is not enabled";
    }
    DCMImageDriver driver;
    std::string filePath = TestTools::getFullTestImagePath("dcm", "private/H01EBB50P-24777");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);
    const int numScenes = slide->getNumScenes();
    EXPECT_EQ(1, numScenes);
    for (int iScene = 0; iScene < numScenes; ++iScene) {
        std::shared_ptr<slideio::CVScene> scene = slide->getScene(iScene);
        EXPECT_TRUE(scene.get() != nullptr);
        EXPECT_EQ(iScene, scene->getSceneIndex());
        EXPECT_EQ(filePath, scene->getFilePath());
		EXPECT_EQ("DCM", scene->getDriverId());
    }
}

TEST(DCMImageDriver, readBlockWSIDirectory)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() <<
            "Skip private test because private dataset is not enabled";
    }
    DCMImageDriver driver;
    std::string slidePath = TestTools::getFullTestImagePath("dcm", "private/H01EBB50P-24777");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    std::string testFilePath = TestTools::getFullTestImagePath("dcm", "private/H01EBB50P-24777.block.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath);
    auto slide = driver.openFile(slidePath);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    cv::Rect rectScene = scene->getRect();
    int x = rectScene.x + rectScene.width / 3;
    int y = rectScene.y + rectScene.height / 3;
    const int width = 600;
    const int height = 400;
    cv::Rect rectBlock = { x, y, width, height };
    cv::Mat raster;
    scene->readBlock(rectBlock, raster);
    cv::Mat testRaster;
    //TestTools::writePNG(raster, testFilePath);
    TestTools::readPNG(testFilePath, testRaster);
    TestTools::compareRasters(raster, testRaster);
    //TestTools::showRasters(testRaster, raster);
}

TEST(DCMImageDriver, readAuxImagesWSIDirectory)
{
    std::list<std::string> auxNames = { "LOCALIZER", "LABEL", "OVERVIEW" };

    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() <<
            "Skip private test because private dataset is not enabled";
    }
    DCMImageDriver driver;
    std::string slidePath = TestTools::getFullTestImagePath("dcm", "private/H01EBB50P-24777");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    std::string testFilePathBase = TestTools::getFullTestImagePath("dcm", "private/H01EBB50P-24777");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePathBase);
    auto slide = driver.openFile(slidePath);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    EXPECT_EQ(3, scene->getNumAuxImages());
    for(auto&& name: auxNames) {
        std::shared_ptr<CVScene> auxScene = scene->getAuxImage(name);
        ASSERT_TRUE(auxScene.get()!=nullptr);
        cv::Mat auxRaster;
        auxScene->readBlock(auxScene->getRect(), auxRaster);
        std::string auxTestPath = testFilePathBase + "." + name + ".png";
        //TestTools::writePNG(auxRaster, auxTestPath);
        cv::Mat testRaster;
        TestTools::readPNG(auxTestPath, testRaster);
        TestTools::compareRasters(auxRaster, testRaster);
        //TestTools::showRaster(auxRaster);
    }
}

TEST(DCMImageDriver, readJp2K)
{
    std::string slidePath = TestTools::getTestImagePath("dcm", "openmicroscopy.org/CT1_J2KI");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    std::string testPath = TestTools::getTestImagePath("dcm", "openmicroscopy.org/CT1_J2KI.tiff");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testPath);

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
    cv::Mat testImage;
    slideio::ImageTools::readSmallImageRaster(testPath, testImage);
    double simScore = ImageTools::computeSimilarity2(image, testImage);
    EXPECT_GT(simScore, 0.999);
}

TEST(DCMImageDriver, zoomLevels)
{
    const slideio::LevelInfo levels[] = {
        slideio::LevelInfo(0, {72192,70400}, 1.0, 0., {256,256}),
        slideio::LevelInfo(1, {36096,35200}, 0.5, 0, {256,256}),
        slideio::LevelInfo(2, {18048,17600}, 0.25, 0, {256,256}),
        slideio::LevelInfo(3, {9024,8800}, 0.125, 0, {256,256}),
        slideio::LevelInfo(4, {4512,4400}, 0.0625, 0, {256,256}),
        slideio::LevelInfo(5, {2256,2200}, 0.03117, 0, {256,256}),
        slideio::LevelInfo(6, {1128,1100}, 0.015625, 0, {256,256}),
        slideio::LevelInfo(7, {564,550}, 0.0078125, 0, {256,256}),
        slideio::LevelInfo(8, {282,275}, 0.00390625, 0, {256,256}),
    };
    slideio::DCMImageDriver driver;
    const std::string filePath = TestTools::getFullTestImagePath("dcm", "private/H01EBB50P-24777");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    const std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    const std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    const int numScenes = slide->getNumScenes();
    const cv::Rect rect = scene->getRect();
    double magnification = scene->getMagnification();
    ASSERT_TRUE(scene != nullptr);
    const int numLevels = scene->getNumZoomLevels();
    ASSERT_EQ(9, numLevels);
    for (int levelIndex = 0; levelIndex < numLevels; ++levelIndex)
    {
        const slideio::LevelInfo* level = scene->getZoomLevelInfo(levelIndex);
        EXPECT_EQ(*level, levels[levelIndex]);
        if (levelIndex == 0) {
            EXPECT_EQ(level->getSize(), Tools::cvSizeToSize(rect.size()));
        }

    }
}

TEST(DCMImageDriver, zoomLevelsSingle)
{
    const slideio::LevelInfo levels[] = {
        slideio::LevelInfo(0, {512,512}, 1.0, 0., {512,512}),
    };
    slideio::DCMImageDriver driver;
    std::string filePath = TestTools::getTestImagePath(
        "dcm", "barre.dev/OT-MONO2-8-hip.dcm");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    const std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    const std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    const int numScenes = slide->getNumScenes();
    const cv::Rect rect = scene->getRect();
    double magnification = scene->getMagnification();
    ASSERT_TRUE(scene != nullptr);
    const int numLevels = scene->getNumZoomLevels();
    ASSERT_EQ(1, numLevels);
    for (int levelIndex = 0; levelIndex < numLevels; ++levelIndex)
    {
        const slideio::LevelInfo* level = scene->getZoomLevelInfo(levelIndex);
        EXPECT_EQ(*level, levels[levelIndex]);
        if (levelIndex == 0) {
            EXPECT_EQ(level->getSize(), Tools::cvSizeToSize(rect.size()));
        }

    }
}

// Behavior preservation: a level read resampled down to a coarser level's size must
// essentially match a scene read resampled to the same size, level by level.
TEST(DCMImageDriver, readLevelMatchesTheResampledSceneRead)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() <<
            "Skip private test because full dataset is not enabled";
    }
    DCMImageDriver driver;
    std::string slidePath = TestTools::getFullTestImagePath("dcm", "private/H01EBB50P-24777");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    auto slide = driver.openFile(slidePath);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);

    const int numLevels = scene->getNumZoomLevels();
    ASSERT_LE(2, numLevels);
    // computeSimilarity2 goes through cv::sum, which only supports up to 4 channels; cap the
    // comparison to a channel subset if the fixture turns out to carry more.
    const int numChannels = scene->getNumChannels();
    const std::vector<int> channelIndices = numChannels > 4 ? std::vector<int>{0, 1, 2} : std::vector<int>{};
    // A whole-level read at the finer levels of this pyramid is tens of megapixels (level 0
    // is 72192x70400), so only the coarsest three levels are exercised here -- they test the
    // same property at a fraction of the cost.
    for (int level = std::max(1, numLevels - 3); level < numLevels; ++level)
    {
        const slideio::LevelInfo* info = scene->getZoomLevelInfo(level);
        ASSERT_TRUE(info != nullptr) << "level " << level;
        const cv::Size levelSize(info->getSize().width, info->getSize().height);
        cv::Mat viaLevel, viaScene;
        scene->readResampledLevelBlockChannels(level, cv::Rect(cv::Point(0, 0), levelSize), levelSize,
                                               channelIndices, viaLevel);
        scene->readResampledBlockChannels(scene->getRect(), levelSize, channelIndices, viaScene);
        ASSERT_EQ(levelSize, viaLevel.size()) << "level " << level;
        EXPECT_LE(0.95, ImageTools::computeSimilarity2(viaLevel, viaScene)) << "level " << level;
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
// 72192x70400, and a whole-level read at that size is prohibitively expensive here. Levels 0
// and 1 (rather than the coarsest pair) are used because they are the most collapse-prone --
// exactly the pair the reference tests for other drivers exercise.
TEST(DCMImageDriver, readLevelDoesNotReuseAdjacentLevel)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() <<
            "Skip private test because full dataset is not enabled";
    }
    DCMImageDriver driver;
    std::string slidePath = TestTools::getFullTestImagePath("dcm", "private/H01EBB50P-24777");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    auto slide = driver.openFile(slidePath);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
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

TEST(DCMImageDriver, multiThreadSceneAccess) {
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() <<
            "Skip the test because full dataset is not enabled";
    }
    std::string filePath = TestTools::getFullTestImagePath("dcm", "private/H01EBB50P-24777");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    DCMImageDriver driver;
    TestTools::multiThreadedTest(filePath, driver);
}
