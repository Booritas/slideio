#include <boost/algorithm/string/predicate.hpp>
#include <gtest/gtest.h>
#include "slideio/drivers/svs/svsimagedriver.hpp"
#include "slideio/drivers/svs/svstiledscene.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include "testtools.hpp"
#include <stdint.h>
#include <functional>
#include <numeric>
#include <vector>

TEST(SVSImageDriver, driverID)
{
    slideio::SVSImageDriver driver;
    EXPECT_EQ(driver.getID(), "SVS");
}

TEST(SVSImageDriver, canOpenFile)
{
    slideio::SVSImageDriver driver;
    EXPECT_TRUE(driver.canOpenFile("abc.svs"));
    EXPECT_FALSE(driver.canOpenFile("abc.tif"));
}

TEST(SVSImageDriver, openFile_BrightField)
{
    slideio::SVSImageDriver driver;
    std::string path = TestTools::getTestImagePath("svs","CMU-1-Small-Region.svs");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(path);
    ASSERT_TRUE(slide!=nullptr);
    int numbScenes = slide->getNumScenes();
    ASSERT_TRUE(numbScenes==1);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    ASSERT_TRUE(scene!=nullptr);
    EXPECT_EQ(slide->getFilePath(),path);
    EXPECT_EQ(scene->getFilePath(),path);
    int channels = scene->getNumChannels();
    EXPECT_EQ(channels, 3);
    cv::Rect sceneRect = scene->getRect();
    EXPECT_EQ(sceneRect.width, 2220);
    EXPECT_EQ(sceneRect.height, 2967);
    EXPECT_EQ(scene->getChannelDataType(0), slideio::DataType::DT_Byte);
    EXPECT_EQ(scene->getChannelDataType(1), slideio::DataType::DT_Byte);
    EXPECT_EQ(scene->getChannelDataType(2), slideio::DataType::DT_Byte);
    slideio::Resolution res = scene->getResolution();
    EXPECT_EQ(res.x, 4.99e-7);
    EXPECT_EQ(res.y, 4.99e-7);
    double magn = scene->getMagnification();
    EXPECT_EQ(20., magn);
}

TEST(SVSImageDriver, read_Thumbnail_WholeImage)
{
    // read image by svs driver
    slideio::SVSImageDriver driver;
    std::string path = TestTools::getTestImagePath("svs","CMU-1-Small-Region.svs");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(path);
    ASSERT_TRUE(slide!=nullptr);
    int numbScenes = slide->getNumScenes();
    ASSERT_TRUE(numbScenes == 1);
    int numbAuxImages = slide->getNumAuxImages();
    ASSERT_TRUE(numbAuxImages == 3);
    const std::list<std::string>& auxImages = slide->getAuxImageNames();
    ASSERT_TRUE(std::find(auxImages.begin(), auxImages.end(), "Thumbnail") != auxImages.end());
    std::shared_ptr<slideio::CVScene> scene = slide->getAuxImage("Thumbnail");
    ASSERT_TRUE(scene!=nullptr);
    cv::Rect sceneRect = scene->getRect();
    cv::Mat imageRaster;
    scene->readBlock(sceneRect, imageRaster);
    
    // read extracted page by GDAL library
    std::string pathPageFile = TestTools::getTestImagePath("svs","CMU-1-Small-Region-page-1.tif");
    cv::Mat pageRaster;
    slideio::ImageTools::readGDALImage(pathPageFile, pageRaster);

    cv::Mat score;
    cv::matchTemplate(imageRaster, pageRaster, score, cv::TM_CCOEFF_NORMED);
    double minScore(0), maxScore(0);
    cv::minMaxLoc(score, &minScore, &maxScore);
    ASSERT_LT(0.99, minScore);
}

TEST(SVSImageDriver, read_Thumbnail_Block)
{
    // read image by svs driver
    slideio::SVSImageDriver driver;
    std::string path = TestTools::getTestImagePath("svs","CMU-1-Small-Region.svs");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(path);
    ASSERT_TRUE(slide!=nullptr);
    int numbScenes = slide->getNumScenes();
    ASSERT_TRUE(numbScenes==1);
    int numbAuxImages = slide->getNumAuxImages();
    ASSERT_TRUE(numbAuxImages == 3);
    const std::list<std::string>& auxImages = slide->getAuxImageNames();
    ASSERT_TRUE(std::find(auxImages.begin(), auxImages.end(), "Thumbnail") != auxImages.end());
    std::shared_ptr<slideio::CVScene> scene = slide->getAuxImage("Thumbnail");
    ASSERT_TRUE(scene!=nullptr);
    cv::Rect sceneRect = scene->getRect();
    int block_sx = sceneRect.width/4;
    int block_sy = sceneRect.height/3;
    int block_x = sceneRect.width/6;
    int block_y = sceneRect.height/5;
    
    cv::Rect blockRect = {block_x,block_y,block_sx, block_sy};
    cv::Mat blockRaster;
    scene->readBlock(blockRect, blockRaster);
    ASSERT_EQ(blockRaster.cols, block_sx);
    ASSERT_EQ(blockRaster.rows, block_sy);

    // read extracted page by GDAL library
    std::string pathPageFile = TestTools::getTestImagePath("svs", "CMU-1-Small-Region-page-1.tif");
    cv::Mat pageRaster;
    slideio::ImageTools::readGDALImage(pathPageFile, pageRaster);
    cv::Mat pageBlockRaster = pageRaster(blockRect);

    cv::Mat score;
    cv::matchTemplate(blockRaster, pageBlockRaster, score, cv::TM_CCOEFF_NORMED);
    double minScore(0), maxScore(0);
    cv::minMaxLoc(score, &minScore, &maxScore);
    ASSERT_LT(0.99, minScore);
}

TEST(SVSImageDriver, read_Thumbnail_BlockWithScale)
{
    // read image by svs driver
    slideio::SVSImageDriver driver;
    std::string path = TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(path);
    ASSERT_TRUE(slide != nullptr);
    int numbScenes = slide->getNumScenes();
    ASSERT_TRUE(numbScenes == 1);
    int numbAuxImages = slide->getNumAuxImages();
    ASSERT_TRUE(numbAuxImages == 3);
    const std::list<std::string>& auxImages = slide->getAuxImageNames();
    ASSERT_TRUE(std::find(auxImages.begin(), auxImages.end(), "Thumbnail") != auxImages.end());
    std::shared_ptr<slideio::CVScene> scene = slide->getAuxImage("Thumbnail");
    ASSERT_TRUE(scene != nullptr);
    cv::Rect sceneRect = scene->getRect();
    int block_sx = sceneRect.width/3;
    int block_sy = sceneRect.height/2;
    int block_x = sceneRect.width/6;
    int block_y = sceneRect.height/5;
    double scale = 0.8;

    cv::Rect blockRect = { block_x,block_y,block_sx, block_sy };
    cv::Size blockSize = { (int)(sceneRect.width * scale), (int)(sceneRect.height * scale) };

    cv::Mat blockRaster;
    scene->readResampledBlock(blockRect, blockSize, blockRaster);
    ASSERT_EQ(blockRaster.cols, blockSize.width);
    ASSERT_EQ(blockRaster.rows, blockSize.height);

    // read extracted page by GDAL library
    std::string pathPageFile = TestTools::getTestImagePath("svs", "CMU-1-Small-Region-page-1.tif");
    cv::Mat pageRaster;
    slideio::ImageTools::readGDALImage(pathPageFile, pageRaster);
    cv::Mat pageBlockRaster = pageRaster(blockRect);
    cv::Mat pageResizedRaster;
    cv::resize(pageBlockRaster, pageResizedRaster, blockSize);

    cv::Mat score;
    cv::matchTemplate(blockRaster, pageResizedRaster, score, cv::TM_CCOEFF_NORMED);
    double minScore(0), maxScore(0);
    cv::minMaxLoc(score, &minScore, &maxScore);
    ASSERT_LT(0.99, minScore);
}

TEST(SVSImageDriver, findZoomDirectory)
{
    std::vector<slideio::TiffDirectory> dirs;
    dirs.resize(10);
    int baseWidth = 38528;
    int baseHeight = 77056;
    int scale = 1;
    int index = 0;
    for(auto& dir: dirs)
    {
        dir.width = baseWidth / scale;
        dir.height = baseHeight / scale;
        dir.dirIndex = index;
        scale *= 2;
        index++;
    }

    std::string fake_path = TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs");
    slideio::SVSTiledScene scene(fake_path, "fake_name", dirs);
    auto& lastDir = dirs[dirs.size()-1];
    const cv::Rect sceneRect = scene.getRect();
    double lastZoom = static_cast<double>(lastDir.width) / static_cast<double>(sceneRect.width);

    EXPECT_EQ(scene.findZoomDirectory(2.).dirIndex, 0);
    EXPECT_EQ(scene.findZoomDirectory(lastZoom).dirIndex, 9);
    EXPECT_EQ(scene.findZoomDirectory(lastZoom*2).dirIndex, 8);
    EXPECT_EQ(scene.findZoomDirectory(lastZoom/2).dirIndex, 9);
    EXPECT_EQ(scene.findZoomDirectory(0.5).dirIndex, 1);
    EXPECT_EQ(scene.findZoomDirectory(0.501).dirIndex, 1);
    EXPECT_EQ(scene.findZoomDirectory(0.499).dirIndex, 1);
    EXPECT_EQ(scene.findZoomDirectory(0.25).dirIndex, 2);
    EXPECT_EQ(scene.findZoomDirectory(0.125).dirIndex, 3);
    EXPECT_EQ(scene.findZoomDirectory(0.55).dirIndex, 0);
    EXPECT_EQ(scene.findZoomDirectory(0.45).dirIndex, 1);
    EXPECT_EQ(scene.findZoomDirectory(0.1).dirIndex, 3);
}

TEST(SVSImageDriver, readBlock_WholeImage)
{
    slideio::SVSImageDriver driver;
    std::string path = TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(path);
    ASSERT_TRUE(slide != nullptr);
    int numbScenes = slide->getNumScenes();
    ASSERT_TRUE(numbScenes == 1);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);
    const cv::Rect sceneRect = scene->getRect();
    ASSERT_EQ(sceneRect.width, 2220);
    ASSERT_EQ(sceneRect.height, 2967);
    cv::Mat sceneRaster;
    scene->readBlock(sceneRect, sceneRaster);

    //namedWindow( "Display window", WINDOW_AUTOSIZE );// Create a window for display.
    //imshow( "Display window", sceneRaster );                   // Show our image inside it.
    //waitKey(0);

    // read extracted page by GDAL library
    std::string pathPageFile = TestTools::getTestImagePath("svs", "CMU-1-Small-Region-page-0.tif");
    cv::Mat pageRaster;
    slideio::ImageTools::readGDALImage(pathPageFile, pageRaster);
    cv::Mat pageBlockRaster = pageRaster(sceneRect);
    cv::Mat score;
    cv::matchTemplate(sceneRaster, pageBlockRaster, score, cv::TM_CCOEFF_NORMED);
    double minScore(0), maxScore(0);
    cv::minMaxLoc(score, &minScore, &maxScore);
    ASSERT_LT(0.99, minScore);
}

TEST(SVSImageDriver, readBlock_Part)
{
    slideio::SVSImageDriver driver;
    std::string path = TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(path);
    ASSERT_TRUE(slide != nullptr);
    int numbScenes = slide->getNumScenes();
    ASSERT_TRUE(numbScenes == 1);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);
    const cv::Rect sceneRect = scene->getRect();
    ASSERT_EQ(sceneRect.width, 2220);
    ASSERT_EQ(sceneRect.height, 2967);
    cv::Mat blockRaster;
    cv::Rect blockRect = {sceneRect.width/2, sceneRect.height/2, 300, 300};
    scene->readBlock(blockRect, blockRaster);

    // read extracted page by GDAL library
    std::string pathPageFile = TestTools::getTestImagePath("svs", "CMU-1-Small-Region-page-0.tif");
    cv::Mat pageRaster;
    slideio::ImageTools::readGDALImage(pathPageFile, pageRaster);
    cv::Mat pageBlockRaster = pageRaster(blockRect);
    cv::Mat score;
    cv::matchTemplate(blockRaster, pageBlockRaster, score, cv::TM_CCOEFF_NORMED);
    double minScore(0), maxScore(0);
    cv::minMaxLoc(score, &minScore, &maxScore);
    ASSERT_LT(0.99, minScore);
}

TEST(SVSImageDriver, readBlock_PartScale)
{
    slideio::SVSImageDriver driver;
    std::string path = TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(path);
    ASSERT_TRUE(slide != nullptr);
    int numbScenes = slide->getNumScenes();
    ASSERT_TRUE(numbScenes == 1);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);
    const cv::Rect sceneRect = scene->getRect();
    ASSERT_EQ(sceneRect.width, 2220);
    ASSERT_EQ(sceneRect.height, 2967);
    cv::Mat blockRaster;
    cv::Rect blockRect = { sceneRect.width / 2, sceneRect.height / 2, 300, 300 };
    cv::Size blockSize = blockRect.size();
    blockSize /= 2;
    scene->readResampledBlock(blockRect, blockSize, blockRaster);

    // read extracted page by GDAL library
    std::string pathPageFile = TestTools::getTestImagePath("svs", "CMU-1-Small-Region-page-0.tif");
    cv::Mat pageRaster;
    slideio::ImageTools::readGDALImage(pathPageFile, pageRaster);
    cv::Mat pageBlockRaster = pageRaster(blockRect);
    cv::Mat scaledRaster;
    cv::resize(pageBlockRaster, scaledRaster, blockSize);
    cv::Mat score;
    cv::matchTemplate(blockRaster, scaledRaster, score, cv::TM_CCOEFF_NORMED);
    double minScore(0), maxScore(0);
    cv::minMaxLoc(score, &minScore, &maxScore);
    EXPECT_LT(0.98, minScore);
}

TEST(SVSImageDriver, metadataCompression)
{
    typedef std::tuple<std::string, int, slideio::Compression> SceneCompression;
    const SceneCompression compressionData[] ={
        SceneCompression("CMU-1-Small-Region.svs",0,slideio::Compression::Jpeg),
        SceneCompression("JP2K-33003-1.svs", 0, slideio::Compression::Jpeg2000),
    };
    slideio::SVSImageDriver driver;
    for(const auto& item: compressionData)
    {

        const std::string& imageName = std::get<0>(item);
        const int sceneIndex = std::get<1>(item);
        const slideio::Compression sceneCompression = std::get<2>(item);

        std::string filePath = TestTools::getTestImagePath("svs",imageName);
        std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
        std::shared_ptr<slideio::CVScene> scene = slide->getScene(sceneIndex);
        EXPECT_TRUE(scene!=nullptr);
        EXPECT_EQ(scene->getCompression(), sceneCompression);
    }    
}

TEST(SVSImageDriver, slideRawMetadata)
{
    const std::string images[] = {
        "CMU-1-Small-Region.svs",
        "JP2K-33003-1.svs"
    };
    slideio::SVSImageDriver driver;
    for(const auto& imageName: images)
    {
        std::string filePath = TestTools::getTestImagePath("svs",imageName);
        std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
        const std::string& metadata = slide->getRawMetadata();
        EXPECT_GT(metadata.length(),0);
        const std::string header("Aperio Image Library");
        EXPECT_TRUE(boost::algorithm::starts_with(metadata, header));
    }    
}

TEST(SVSImageDriver, crashTest)
{
    slideio::SVSImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("svs","corrupted.svs");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    int numScenes = slide->getNumScenes();
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    cv::Rect rect = scene->getRect();
    cv::Mat block;
    cv::Rect blockRect = {0, 0, 1000,1000};
    EXPECT_NO_THROW(scene->readBlock(blockRect, block));
}

TEST(SVSImageDriver, swapedChannels)
{
    slideio::SVSImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    cv::Mat matSwaped, matOrigin;
    cv::Rect rect = scene->getRect();
    std::vector<int> channels = { 2,1,0 };
    cv::Size size = { rect.width / 5, rect.height / 5 };
    scene->readResampledBlock(rect, size, matOrigin);
    scene->readResampledBlockChannels(rect, size, channels, matSwaped);
    int channelSize = size.width * size.height;
   
    for(int swapedIndex=0; swapedIndex<3; ++swapedIndex)
    {
        int originIndex = channels[swapedIndex];
        cv::Mat originChannel, swapedChannel;
        cv::extractChannel(matOrigin, originChannel, originIndex);
        cv::extractChannel(matSwaped, swapedChannel, swapedIndex);
        EXPECT_EQ(std::memcmp(originChannel.data, swapedChannel.data, channelSize), 0);
    }
}

TEST(SVSImageDriver, imageResolution)
{
    slideio::SVSImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    slideio::Resolution res = scene->getResolution();
    EXPECT_DOUBLE_EQ(res.x, 0.4990e-6);
    EXPECT_DOUBLE_EQ(res.y, 0.4990e-6);
}

TEST(SVSImageDriver, imageResolutionPrivate)
{
    if(!TestTools::isPrivateTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because private dataset is not enabled";
    }
    slideio::SVSImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("svs", "jp2k_3chnl_8bit.svs", true);
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    slideio::Resolution res = scene->getResolution();
    EXPECT_DOUBLE_EQ(res.x, 0.23250e-6);
    EXPECT_DOUBLE_EQ(res.y, 0.23250e-6);
}

TEST(SVSImageDriver, auxImages)
{
    // read image by svs driver
    slideio::SVSImageDriver driver;
    std::string path = TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(path);
    ASSERT_TRUE(slide != nullptr);
    int numbScenes = slide->getNumScenes();
    ASSERT_TRUE(numbScenes == 1);
    int numbAuxImages = slide->getNumAuxImages();
    ASSERT_TRUE(numbAuxImages == 3);
    const std::list<std::string>& auxImages = slide->getAuxImageNames();
    ASSERT_TRUE(std::find(auxImages.begin(), auxImages.end(), "Thumbnail") != auxImages.end());
    ASSERT_TRUE(std::find(auxImages.begin(), auxImages.end(), "Macro") != auxImages.end());
    ASSERT_TRUE(std::find(auxImages.begin(), auxImages.end(), "Label") != auxImages.end());
    // Thumbnail
    std::shared_ptr<slideio::CVScene> thumbnail = slide->getAuxImage("Thumbnail");
    ASSERT_TRUE(thumbnail != nullptr);
    cv::Rect sceneRect = thumbnail->getRect();
    ASSERT_EQ(sceneRect, cv::Rect(0, 0, 574, 768));
    cv::Mat thumbRaster;
    thumbnail->readBlock(sceneRect, thumbRaster);
    ASSERT_EQ(thumbRaster.cols, sceneRect.width);
    ASSERT_EQ(thumbRaster.rows, sceneRect.height);

    // Macro
    std::shared_ptr<slideio::CVScene> macro = slide->getAuxImage("Macro");
    ASSERT_TRUE(macro != nullptr);
    sceneRect = macro->getRect();
    ASSERT_EQ(sceneRect, cv::Rect(0, 0, 1280, 431));
    cv::Mat macroRaster;
    macro->readBlock(sceneRect, macroRaster);
    ASSERT_EQ(macroRaster.cols, sceneRect.width);
    ASSERT_EQ(macroRaster.rows, sceneRect.height);

    // Label
    std::shared_ptr<slideio::CVScene> label = slide->getAuxImage("Label");
    ASSERT_TRUE(label != nullptr);
    sceneRect = label->getRect();
    ASSERT_EQ(sceneRect, cv::Rect(0, 0, 387, 463));
    cv::Mat labelRaster;
    thumbnail->readBlock(sceneRect, labelRaster);
    ASSERT_EQ(labelRaster.cols, sceneRect.width);
    ASSERT_EQ(labelRaster.rows, sceneRect.height);
}
