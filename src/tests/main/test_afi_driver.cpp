#include "tests/testlib/testtools.hpp"
#include "slideio/drivers/afi/afiimagedriver.hpp"
#include "slideio/drivers/afi/afislide.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/base/exceptions.hpp"

#include <gtest/gtest.h>
#include <filesystem>
#include <opencv2/imgproc.hpp>

#include <numeric>


static std::string getPrivTestImagesPath(std::string dir, std::string file)
{
    return TestTools::getTestImagePath(dir, file, true);
}

TEST(AFIDriver, driverID)
{
    slideio::AFIImageDriver driver;
    EXPECT_EQ(driver.getID(), "AFI");
}

TEST(AFIDriver, canOpenFile)
{
    slideio::AFIImageDriver driver;
    EXPECT_TRUE(driver.canOpenFile("C:\\Users\\images\\51445.afi"));
    EXPECT_FALSE(driver.canOpenFile("abc.svs"));
}

TEST(AFIDriver, readFileEntries)
{
    const auto files = slideio::AFISlide::getFileList("<ImageList>\
        <Image>\
        <Path>51445_Alexa Fluor 594.svs</Path>\
        <ID>@51446</ID >\
        </Image>\
        <Image>\
        <Path>51445_Alexa Fluor 488.svs</Path>\
        <ID>@51447 </ID>\
        </Image>\
        </ImageList>");
    EXPECT_EQ(files.size(), 2);
}

TEST(AFIDriver, readFileBrokenEntries)
{
    EXPECT_THROW(slideio::AFISlide::getFileList("<ImageList>\
        <Image>\
        <Path>51445_Alexa Fluor 594.svs</Path>\
        <ID>@51446</ID >"), slideio::RuntimeError);
}

class AFIDriverFileTest : public ::testing::Test {
public:
    void SetUp() override {
        if (!TestTools::isPrivateTestEnabled()) {
            GTEST_SKIP() << "Skip private test because private dataset is not enabled";
        }
    }
};

TEST_F(AFIDriverFileTest, openFile)
{
    slideio::AFIImageDriver driver;
    const std::string filePath = getPrivTestImagesPath("afi", "fs.afi");
    auto slide = driver.openFile(filePath);
    EXPECT_TRUE(slide!=nullptr);
    EXPECT_EQ(slide->getMetadataFormat(), slideio::MetadataFormat::None);
	EXPECT_EQ(slide->getScene(0)->getMetadataFormat(), slideio::MetadataFormat::None);
}

TEST_F(AFIDriverFileTest, getScenesFromFiles)
{
    std::vector<std::string> svsFiles = { "fs_Alexa Fluor 594.svs", "fs_Alexa Fluor 488.svs", "fs_DAPI.svs" };
    const std::string afiFile = getPrivTestImagesPath("afi", "fs.afi");
    auto slidesScenes = slideio::AFISlide::getSlidesScenesFromFiles(svsFiles, afiFile);
    EXPECT_EQ(slidesScenes.first.size(), 3);
    EXPECT_EQ(slidesScenes.second.size(), 3);
}

TEST_F(AFIDriverFileTest, getScenesFromNonExistentFiles)
{
    std::vector<std::string> svsFiles = { "FakeFile1.svs", "FakeFile2.svs" };
    const std::string afiFile = getPrivTestImagesPath("afi", "fs.afi");
    EXPECT_THROW(slideio::AFISlide::getSlidesScenesFromFiles(svsFiles, afiFile), slideio::RuntimeError);
}

TEST_F(AFIDriverFileTest, checkFile)
{
    slideio::AFIImageDriver driver;
    const std::string filePath = getPrivTestImagesPath("afi", "fs.afi");
    auto slide = driver.openFile(filePath);
    EXPECT_TRUE(slide != nullptr);
    EXPECT_EQ(slide->getNumScenes(), 3);
    EXPECT_EQ(slide->getFilePath(), filePath);
    auto scene = slide->getScene(1);
    EXPECT_EQ(scene->getName(), "Image");
    std::string scenePath = std::filesystem::path(scene->getFilePath()).lexically_normal().string();
    std::string svsPath = getPrivTestImagesPath("afi", "fs_Alexa Fluor 488.svs");
    EXPECT_EQ(scenePath, svsPath);
}

TEST_F(AFIDriverFileTest, read_ImageBlock)
{
    // read image by afi driver
    slideio::AFIImageDriver driver;
    std::string path = getPrivTestImagesPath("afi", "fs.afi");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(path);
    ASSERT_TRUE(slide != nullptr);

    std::shared_ptr<slideio::CVScene> scene = slide->getScene(1);
    ASSERT_TRUE(scene != nullptr);
    cv::Rect sceneRect{2500, 4000, 400, 400};
    cv::Mat imageRaster;
    scene->readBlock(sceneRect, imageRaster);
    const std::string pathPageFile = getPrivTestImagesPath("afi", "fs_Alexa Fluor 488_block_2500_4000_400_400.tif");
    cv::Mat refRaster;
    slideio::ImageTools::readGDALImage(pathPageFile, refRaster);
	double sim = slideio::ImageTools::computeSimilarity2(imageRaster, refRaster);
	EXPECT_GT(sim, 0.999);
    //TestTools::showRasters(imageRaster, refRaster);
}

TEST_F(AFIDriverFileTest, read_ImageBlockScaled)
{
    slideio::AFIImageDriver driver;
    std::string path = getPrivTestImagesPath("afi", "fs.afi");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(path);
    ASSERT_TRUE(slide != nullptr);

    std::shared_ptr<slideio::CVScene> scene = slide->getScene(1);
    ASSERT_TRUE(scene != nullptr);
    cv::Rect sceneRect{ 2500, 4000, 400, 400 };
    cv::Size blockSize{ 800,800 };
    cv::Mat blockRaster;
    scene->readResampledBlock(sceneRect, blockSize , blockRaster);
    cv::Mat imageRaster32bit;
    blockRaster.convertTo(imageRaster32bit, CV_32FC1);
    const std::string pathPageFile = getPrivTestImagesPath("afi", "fs_Alexa Fluor 488_block_2500_4000_400_400.tif");
    cv::Mat refRaster;
    slideio::ImageTools::readGDALImage(pathPageFile, refRaster);
    cv::Mat scaledRaster;
    cv::resize(refRaster, scaledRaster, blockSize);
    double sim = slideio::ImageTools::computeSimilarity2(blockRaster, scaledRaster);
    EXPECT_GT(sim, 0.999);
    //TestTools::showRasters(blockRaster, scaledRaster);
}

TEST_F(AFIDriverFileTest, multiThreadSceneAccess) {
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() <<
            "Skip the test because full dataset is not enabled";
    }
    std::string filePath = getPrivTestImagesPath("afi", "fs.afi");
    slideio::AFIImageDriver driver;
    TestTools::multiThreadedTest(filePath, driver);
}
