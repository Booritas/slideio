// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>


//#include <opencv2/highgui.hpp>
#include "slideio-opencv/imgproc.hpp"


#include "slideio/core/tools/tools.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"
#include "tests/testlib/testtools.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/imagetools/cvtools.hpp"
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
        "dcm", "barre.dev/OT-MONO2-8-hip.dcm");
    std::string testPath = TestTools::getTestImagePath(
        "dcm", "barre.dev/OT-MONO2-8-hip.frames/frame0.png");

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

TEST(DCMImageDriver, getRawMetadata)
{
    std::string slidePath = TestTools::getTestImagePath(
        "dcm", "barre.dev/OT-MONO2-8-hip.dcm");

    DCMImageDriver driver;
    auto slide = driver.openFile(slidePath);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 1);
    auto scene = slide->getScene(0);
    std::string metadata = scene->getRawMetadata();
    ASSERT_LT(2, metadata.length());
    EXPECT_EQ('{', metadata.front());
    EXPECT_EQ('}', metadata.back());
}


TEST(DCMImageDriver, readSimpleFileResampled)
{
    std::string slidePath = TestTools::getTestImagePath(
        "dcm", "barre.dev/OT-MONO2-8-hip.dcm");
    std::string testPath = TestTools::getTestImagePath(
        "dcm", "barre.dev/OT-MONO2-8-hip.frames/frame0.png");

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
    std::string slidePath = TestTools::getTestImagePath("dcm", "barre.dev/XA-MONO2-8-12x-catheter");
    std::string testPath1 = TestTools::getTestImagePath("dcm", "barre.dev/XA-MONO2-8-12x-catheter.frames/frame5.png");
    std::string testPath2 = TestTools::getTestImagePath("dcm", "barre.dev/XA-MONO2-8-12x-catheter.frames/frame6.png");

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
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() <<
            "Skip the test because full dataset is not enabled";
    }
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

TEST(DCMImageDriver, readBlockChangingBits)
{
    std::string slidePath = TestTools::getTestImagePath("dcm", "barre.dev/US-PAL-8-10x-echo");
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
    std::string testFilePath = TestTools::getFullTestImagePath(
        "dcm", "private/wsi/M01FBC14P-589_level-0.block.dcm");
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
    std::string testFilePath = TestTools::getFullTestImagePath(
        "dcm", "private/wsi/M01FBC14P-589_level-0.block.dcm");
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
    std::string testFilePath1 = TestTools::getFullTestImagePath("dcm", "private/H01EBB50P-24777.block-2.png");
    std::string testFilePath2 = TestTools::getFullTestImagePath("dcm", "private/H01EBB50P-24777.block-3.png");
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

TEST(DCMImageDriver, readBlockWSIDirectory)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() <<
            "Skip private test because private dataset is not enabled";
    }
    DCMImageDriver driver;
    std::string slidePath = TestTools::getFullTestImagePath("dcm", "private/H01EBB50P-24777");
    std::string testFilePath = TestTools::getFullTestImagePath("dcm", "private/H01EBB50P-24777.block.png");
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
    std::string testFilePathBase = TestTools::getFullTestImagePath("dcm", "private/H01EBB50P-24777");
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
    std::string testPath = TestTools::getTestImagePath("dcm", "openmicroscopy.org/CT1_J2KI.tiff");

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
    slideio::ImageTools::readGDALImage(testPath, testImage);
    TestTools::compareRasters(image, testImage);
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
