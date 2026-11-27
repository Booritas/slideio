#include "slideio/drivers/gdal/gdalimagedriver.hpp"
#include "tests/testlib/testtools.hpp"
#include "slideio/slideio/sceneinternal.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include "slideio/slideio/slideio.hpp"

#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <numeric>
#include <tinyxml2.h>
#include <nlohmann/json.hpp>

#include "slideio/imagetools/imagetools.hpp"
#include "slideio/imagetools/libtiff.hpp"
#include "slideio/core/tools/tempfile.hpp"

using namespace tinyxml2;
using json = nlohmann::json;

#include <gtest/gtest.h>
#include <numeric>
#include <tinyxml2.h>
#include <nlohmann/json.hpp>

using namespace tinyxml2;
using json = nlohmann::json;

TEST(GDALDriver, driverID)
{
    slideio::GDALImageDriver driver;
    EXPECT_EQ(driver.getID(), "GDAL");
}

TEST(GDALDriver, canOpenFile)
{
    slideio::GDALImageDriver driver;
    EXPECT_TRUE(driver.canOpenFile("abc.png"));
    EXPECT_FALSE(driver.canOpenFile("abc.svs"));
}

TEST(GDALDriver, openPngFile_3chnls_8bit)
{
    slideio::GDALImageDriver driver;
    std::string path = TestTools::getTestImagePath("gdal","img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
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
    EXPECT_EQ(sceneRect.width, 2448);
    EXPECT_EQ(sceneRect.height, 2448);
    EXPECT_EQ(scene->getChannelDataType(0), slideio::DataType::DT_Byte);
    EXPECT_EQ(scene->getChannelDataType(1), slideio::DataType::DT_Byte);
    EXPECT_EQ(scene->getChannelDataType(2), slideio::DataType::DT_Byte);
    slideio::Resolution res = scene->getResolution();
    EXPECT_NEAR(res.x, 0.0003528, 0.0000001);
    EXPECT_NEAR(res.y, 0.0003528, 0.0000001);
}

TEST(GDALDriver, openPngFile_1chnl_8bit)
{
    slideio::GDALImageDriver driver;
    std::string path = TestTools::getTestImagePath("gdal","img_2448x2448_1x8bit_SRC_GRAY_ducks.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(path);
    ASSERT_TRUE(slide!=nullptr);
    int numbScenes = slide->getNumScenes();
    ASSERT_TRUE(numbScenes==1);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    ASSERT_TRUE(scene!=nullptr);
    EXPECT_EQ(slide->getFilePath(),path);
    EXPECT_EQ(scene->getFilePath(),path);
    int channels = scene->getNumChannels();
    EXPECT_EQ(channels, 1);
    cv::Rect sceneRect = scene->getRect();
    EXPECT_EQ(sceneRect.width, 2448);
    EXPECT_EQ(sceneRect.height, 2448);
    EXPECT_EQ(scene->getChannelDataType(0), slideio::DataType::DT_Byte);
    slideio::Resolution res = scene->getResolution();
    EXPECT_NEAR(res.x, 0.0003528, 0.0000001);
    EXPECT_NEAR(res.y, 0.0003528, 0.0000001);
}

TEST(GDALDriver, openPngFile_3chnl_16bit)
{
    slideio::GDALImageDriver driver;
    std::string path = TestTools::getTestImagePath("gdal","img_2448x2448_3x16bit_SRC_RGB_ducks.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
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
    EXPECT_EQ(sceneRect.width, 2448);
    EXPECT_EQ(sceneRect.height, 2448);
    EXPECT_EQ(scene->getChannelDataType(0), slideio::DataType::DT_UInt16);
    EXPECT_EQ(scene->getChannelDataType(1), slideio::DataType::DT_UInt16);
    EXPECT_EQ(scene->getChannelDataType(2), slideio::DataType::DT_UInt16);
    slideio::Resolution res = scene->getResolution();
    EXPECT_NEAR(res.x, 0.0003528, 0.0000001);
    EXPECT_NEAR(res.y, 0.0003528, 0.0000001);
}

TEST(GDALDriver, openJpegFile_3chnl_8bit)
{
    slideio::GDALImageDriver driver;
    std::string path = TestTools::getTestImagePath("gdal","Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
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
    EXPECT_EQ(sceneRect.width, 5494);
    EXPECT_EQ(sceneRect.height, 5839);
    EXPECT_EQ(scene->getChannelDataType(0), slideio::DataType::DT_Byte);
    EXPECT_EQ(scene->getChannelDataType(1), slideio::DataType::DT_Byte);
    EXPECT_EQ(scene->getChannelDataType(2), slideio::DataType::DT_Byte);
    slideio::Resolution res = scene->getResolution();
    EXPECT_NEAR(res.x, 0.0003528, 0.0000001);
    EXPECT_NEAR(res.y, 0.0003528, 0.0000001);
}

TEST(GDALDriver, readBlockPng)
{
    slideio::GDALImageDriver driver;
    std::string path = TestTools::getTestImagePath("gdal","img_1024x600_3x8bit_RGB_color_bars_CMYKWRGB.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::string referenceImagePath = TestTools::getTestImagePath("gdal", "img_1024x600_3x8bit_RGB_color_bars_CMYKWRGB.bmp");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(referenceImagePath);
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(path);
    ASSERT_TRUE(slide!=nullptr);
    int numbScenes = slide->getNumScenes();
    ASSERT_TRUE(numbScenes==1);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    ASSERT_TRUE(scene!=nullptr);
    int channels = scene->getNumChannels();
    EXPECT_EQ(channels, 3);
    cv::Rect sceneRect = scene->getRect();
    EXPECT_EQ(sceneRect.width, 1024);
    EXPECT_EQ(sceneRect.height, 600);
    EXPECT_EQ(scene->getChannelDataType(0), slideio::DataType::DT_Byte);
    cv::Rect block = {260,500,100,100};
    cv::Mat blockRaster;

    scene->readBlock(block, blockRaster);
    cv::Mat refImage;
    slideio::ImageTools::readBitmap(referenceImagePath, refImage);
	cv::Mat refBlock = refImage(block);
    cv::Scalar colorMean, colorStddev;
    cv::meanStdDev(blockRaster, colorMean, colorStddev);
    EXPECT_EQ(colorMean[0],255);
    EXPECT_NEAR(colorStddev[0],0, 1e-5);

    EXPECT_EQ(colorMean[1], 255);
    EXPECT_NEAR(colorStddev[1], 0, 1.e-5);

    EXPECT_EQ(colorMean[2], 0);
    EXPECT_NEAR(colorStddev[2], 0, 1.e-5);

    cv::Mat channelRaster;
    std::vector<int> channelIndices = { 1 };
    scene->readBlockChannels(block, channelIndices, channelRaster);

    cv::Scalar channelMean, channelStddev;
    cv::meanStdDev(channelRaster, channelMean, channelStddev);
    EXPECT_EQ(channelMean[0], 255);
    EXPECT_NEAR(channelStddev[0], 0, 1e-5);
}

TEST(GDALDriver, readBlockPngResampling)
{
    slideio::GDALImageDriver driver;
    std::string path = TestTools::getTestImagePath("gdal","img_1024x600_3x8bit_RGB_color_bars_CMYKWRGB.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(path);
    ASSERT_TRUE(slide!=nullptr);
    int numbScenes = slide->getNumScenes();
    ASSERT_TRUE(numbScenes==1);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    ASSERT_TRUE(scene!=nullptr);
    int channels = scene->getNumChannels();
    EXPECT_EQ(channels, 3);
    cv::Rect rectScene = scene->getRect();
    EXPECT_EQ(rectScene.width, 1024);
    EXPECT_EQ(rectScene.height, 600);
    EXPECT_EQ(scene->getChannelDataType(0), slideio::DataType::DT_Byte);
    cv::Rect blockRect = {260,500, 100,100};
    cv::Size blockSize = {12,12};
    cv::Mat blockRaster;
    scene->readResampledBlock(blockRect, blockSize, blockRaster);
    cv::Scalar colorMean, colorStddev;
    cv::meanStdDev(blockRaster, colorMean, colorStddev);
    EXPECT_EQ(colorMean[0], 255);
    EXPECT_EQ(colorStddev[0], 0);

    EXPECT_EQ(colorMean[1], 255);
    EXPECT_EQ(colorStddev[1], 0);

    EXPECT_EQ(colorMean[2], 0);
    EXPECT_EQ(colorStddev[2], 0);
}

TEST(GDALDriver, metadataCompression)
{
    slideio::GDALImageDriver driver;
    typedef std::tuple<std::string, slideio::Compression> Data;
    const Data imageData[] = {
        Data("img_1024x600_3x8bit_RGB_color_bars_CMYKWRGB.png", slideio::Compression::Png),
        Data("Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg", slideio::Compression::Jpeg)
    };
    for(const auto& item : imageData)
    {
        std::string filePath = TestTools::getTestImagePath("gdal",std::get<0>(item));
        SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
        std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
        const slideio::Compression sceneCompression = std::get<1>(item);
        std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
        EXPECT_TRUE(scene!=nullptr);
        EXPECT_EQ(scene->getCompression(), sceneCompression);
    }
}

TEST(GDALDriver, read16bitSignedImage)
{
    // Should not throw exception. See issue #5
    // Gdal driver throws an error during reading of signed 16bit image
    std::string path = TestTools::getTestImagePath("zvi", "Zeiss-1-Merged-ch1.tif");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, "GDAL");
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    std::vector<uint8_t> buffer(2 * 1480 * 1132);
    auto rect = scene->getRect();
    scene->readBlock(rect, buffer.data(), buffer.size());
}

TEST(GDALDriver, openFileUtf8)
{
    {
        std::string filePath = TestTools::getTestImagePath("gdal", u8"тест/тест.tif");
        SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
        std::shared_ptr<slideio::Slide> slide = slideio::openSlide(filePath, "GDAL");
        int dirCount = slide->getNumScenes();
        ASSERT_EQ(dirCount, 1);
        std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
        auto rect = scene->getRect();
        std::tuple<int, int, int, int> expectedRect(0, 0, 387, 463);
        EXPECT_EQ(rect, expectedRect);
        const slideio::DataType dt = scene->getChannelDataType(0);
        const int ds = slideio::CVTools::cvGetDataTypeSize(dt);
        int rasterSize = std::get<2>(rect) * std::get<3>(rect) * scene->getNumChannels() * ds;
        std::vector<uint8_t> raster(rasterSize);
        scene->readBlock(rect, raster.data(), raster.size());
    }
    {
        std::string filePath = TestTools::getTestImagePath("unicode", u8"тест/lena_256.jpg");
        SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
        std::shared_ptr<slideio::Slide> slide = slideio::openSlide(filePath, "GDAL");
        int dirCount = slide->getNumScenes();
        ASSERT_EQ(dirCount, 1);
        std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
        auto rect = scene->getRect();
        std::tuple<int, int, int, int> expectedRect(0, 0, 256, 256);
        EXPECT_EQ(rect, expectedRect);
        const slideio::DataType dt = scene->getChannelDataType(0);
        const int ds = slideio::CVTools::cvGetDataTypeSize(dt);
        int rasterSize = std::get<2>(rect) * std::get<3>(rect) * scene->getNumChannels() * ds;
        std::vector<uint8_t> raster(rasterSize);
        scene->readBlock(rect, raster.data(), raster.size());
    }
}

TEST(GDALDriver, metadataTiff)
{
        std::string filePath = TestTools::getTestImagePath("ometiff", u8"SPIM-ModuloAlongZ.ome.tiff");
        SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
        std::shared_ptr<slideio::Slide> slide = slideio::openSlide(filePath, "GDAL");
        std::string metadata = slide->getScene(0)->getRawMetadata();
        EXPECT_EQ(slide->getScene(0)->getMetadataFormat(), slideio::MetadataFormat::JSON);
		EXPECT_FALSE(metadata.empty());
        EXPECT_NE(std::string::npos, metadata.find("<?xml"));
}

TEST(GDALDriver, metadataJpeg)
{
    std::string filePath = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(filePath, "GDAL");
    std::string metadata = slide->getRawMetadata();
    EXPECT_EQ(slide->getMetadataFormat(), slideio::MetadataFormat::JSON);
    EXPECT_FALSE(metadata.empty());
    json jMtd = json::parse(metadata);
    EXPECT_GT(jMtd.size(), 2);
	auto mtd = slide->getMetadata();
    EXPECT_EQ(mtd.size(), 20);
}

TEST(GDALDriver, multiThreadSceneAccess) {
    std::string filePath = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::GDALImageDriver driver;
    TestTools::multiThreadedTest(filePath, driver);
}

// GDAL images have no pyramid, but a scene with no level at all cannot be addressed by
// level, so it reports the single level it is. Reading that level is the same read as
// reading the scene.
TEST(GDALDriver, singleZoomLevelAndLevelRead)
{
    std::string path = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    slideio::GDALImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(path);
    ASSERT_TRUE(slide != nullptr);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);

    ASSERT_EQ(1, scene->getNumZoomLevels());
    const slideio::LevelInfo* level = scene->getZoomLevelInfo(0);
    ASSERT_TRUE(level != nullptr);
    EXPECT_EQ(0, level->getLevel());
    EXPECT_DOUBLE_EQ(1.0, level->getScale());
    EXPECT_EQ(2448, level->getSize().width);
    EXPECT_EQ(2448, level->getSize().height);

    const cv::Rect roi(100, 200, 300, 400);
    cv::Mat viaBlock, viaLevel;
    scene->readBlock(roi, viaBlock);
    scene->readResampledLevelBlockChannels(0, roi, roi.size(), {}, viaLevel);
    ASSERT_EQ(viaBlock.size(), viaLevel.size());
    ASSERT_EQ(viaBlock.type(), viaLevel.type());
    cv::Mat diff;
    cv::absdiff(viaBlock, viaLevel, diff);
    EXPECT_EQ(0, cv::countNonZero(diff.reshape(1)));
}

// A rect running off the right and bottom edges is the ordinary edge-tile case. GDAL crops
// with cv::Mat(raster, rect), which throws out of range, so the clamp in the base class is
// what keeps this from being an exception.
TEST(GDALDriver, levelReadClampsAnOverhangingRect)
{
    std::string path = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    slideio::GDALImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(path);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);

    cv::Mat raster;
    ASSERT_NO_THROW(scene->readResampledLevelBlockChannels(
        0, cv::Rect(2400, 2400, 256, 256), cv::Size(256, 256), {}, raster));
    ASSERT_EQ(cv::Size(256, 256), raster.size());
    // The overhang is background: 255 for a byte image.
    EXPECT_EQ(cv::Vec3b(255, 255, 255), raster.at<cv::Vec3b>(200, 200));
}

TEST(GDALDriver, getSceneIndex)
{
    std::string filePath = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    auto slide = slideio::openSlide(filePath, "AUTO");
    ASSERT_TRUE(slide);
    EXPECT_EQ("GDAL", slide->getDriverId());
    const int numScenes = slide->getNumScenes();
    EXPECT_EQ(1, numScenes);
    for (int iScene = 0; iScene < numScenes; ++iScene) {
        std::shared_ptr<slideio::CVScene> scene = slideio::SceneInternal::getCVScene(slide->getScene(iScene));
        EXPECT_TRUE(scene.get() != nullptr);
        EXPECT_EQ(iScene, scene->getSceneIndex());
        EXPECT_EQ(filePath, scene->getFilePath());
		EXPECT_EQ("GDAL", scene->getDriverId());
    }
}

// img_2448x2448_3x8bit_SRC_RGB_ducks.png carries no ICC profile (confirmed with
// `identify -verbose`), unlike colors.png in this same corpus directory, which
// does -- so this file, not colors.png, is the genuine absent-profile fixture.
TEST(GDALImageDriver, colorProfileAbsentWhenNoIccChunkIsPresent)
{
    std::string path = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, "GDAL");
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    ASSERT_TRUE(scene->getColorProfile().isEmpty());
    ASSERT_EQ(slideio::ColorProfileSource::None, scene->getColorProfile().getSource());
}

// Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg carries a real embedded sRGB
// ICC profile (confirmed with `identify -verbose`: Profile-icc, 3144 bytes,
// "sRGB IEC61966-2.1"). This is read through FIWrapper/FreeImage, exercising
// FreeImage_GetICCProfile -- the JPEG/PNG/BMP/... path.
TEST(GDALImageDriver, colorProfileDecodedFromEmbeddedIccInJpeg)
{
    std::string path = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, "GDAL");
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    const slideio::ColorProfile profile = scene->getColorProfile();
    ASSERT_FALSE(profile.isEmpty());
    ASSERT_EQ(slideio::ColorProfileSource::Embedded, profile.getSource());
    const slideio::ColorProfileInfo info = scene->getColorProfileInfo();
    ASSERT_TRUE(info.present);
    ASSERT_EQ(slideio::IccColorSpace::RGB, info.dataSpace);
}

// test.tif carries the same embedded sRGB ICC profile (Profile-icc, 3144
// bytes). A ".tif" file opens through SmallTiffWrapper rather than FIWrapper
// (see ImageTools::openSmallImage), so this exercises the other backing
// reader: TiffDirectory::iccProfile, populated by TiffTools::scanTiffDirTags.
TEST(GDALImageDriver, colorProfileDecodedFromEmbeddedIccInTiff)
{
    std::string path = TestTools::getTestImagePath("gdal", "test.tif");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, "GDAL");
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    const slideio::ColorProfile profile = scene->getColorProfile();
    ASSERT_FALSE(profile.isEmpty());
    ASSERT_EQ(slideio::ColorProfileSource::Embedded, profile.getSource());
    const slideio::ColorProfileInfo info = scene->getColorProfileInfo();
    ASSERT_TRUE(info.present);
    ASSERT_EQ(slideio::IccColorSpace::RGB, info.dataSpace);
}

TEST(GDALDriver, anExifFileChangeDateIsNotAnAcquisitionTime)
{
    // The one file in this corpus that carries exif states DateTime (0x0132)
    // 2015:02:05 16:27:58 and no DateTimeOriginal. Exif defines 0x0132 as the
    // file change date -- FreeImage labels it "File change date and time" -- so
    // it is not when the image was acquired and the scene reports none. 0x9003,
    // DateTimeOriginal, is the one that means acquisition, and no file here
    // states it.
    slideio::GDALImageDriver driver;
    const std::string path = TestTools::getTestImagePath(
        "gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(path);
    ASSERT_TRUE(slide != nullptr);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);
    EXPECT_EQ(scene->getAcquisitionTime(), 0LL);
}

TEST(GDALDriver, aPngModificationTimeIsNotAnAcquisitionTime)
{
    // colors.png carries a tIME chunk reading 2023-05-27 20:35:52, which would
    // be 1685219752. tIME is the image's last modification, which the png spec
    // says in so many words, and re-saving a file moves it. It is not when the
    // image was acquired, so the scene reports none.
    slideio::GDALImageDriver driver;
    const std::string path = TestTools::getTestImagePath("gdal", "colors.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(path);
    ASSERT_TRUE(slide != nullptr);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);
    EXPECT_EQ(scene->getAcquisitionTime(), 0LL);
}

TEST(GDALDriver, acquisitionTimeFromATiffDateTime)
{
    // The tiff backed half of the driver. No tiff in the corpus carries tag
    // 306, so one is written here: SmallTiffPage reads the value through the
    // same TiffDirectory field the pke driver added.
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    {
        libtiff::TIFF* tiff = libtiff::TIFFOpen(path.c_str(), "w");
        ASSERT_TRUE(tiff != nullptr);
        const int width = 8, height = 8;
        libtiff::TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, width);
        libtiff::TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, height);
        libtiff::TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, 1);
        libtiff::TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, 8);
        libtiff::TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
        libtiff::TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        libtiff::TIFFSetField(tiff, TIFFTAG_ROWSPERSTRIP, height);
        libtiff::TIFFSetField(tiff, TIFFTAG_DATETIME, "2015:02:05 16:27:58");
        std::vector<uint8_t> row(width, 0);
        for (int y = 0; y < height; ++y) {
            ASSERT_EQ(1, libtiff::TIFFWriteScanline(tiff, row.data(), y, 0));
        }
        libtiff::TIFFClose(tiff);
    }

    slideio::GDALImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(path);
    ASSERT_TRUE(slide != nullptr);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);
    EXPECT_EQ(scene->getAcquisitionTime(), 1423153678LL);
}

TEST(GDALDriver, noSignificantBitsOrPlaneTimestamps)
{
    // Neither tiff, png, jpeg nor bmp states how many of the stored bits carry
    // data: BitsPerSample and the png bit depth are the storage width
    // getChannelDataType() already reports, and no corpus png carries the sBIT
    // chunk that would say otherwise. One page is one plane, so there is
    // nothing for a per-plane timestamp to distinguish either.
    slideio::GDALImageDriver driver;
    const std::string path = TestTools::getTestImagePath(
        "gdal", "img_2448x2448_3x16bit_SRC_RGB_ducks.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(path);
    ASSERT_TRUE(slide != nullptr);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);
    ASSERT_EQ(slideio::DataType::DT_UInt16, scene->getChannelDataType(0));
    for (int channel = 0; channel < scene->getNumChannels(); ++channel) {
        EXPECT_EQ(0, scene->getChannelSignificantBits(channel)) << "channel " << channel;
    }
    EXPECT_FALSE(scene->hasPlaneTimestamps());
    EXPECT_DOUBLE_EQ(0., scene->getPlaneTimestamp(0, 0, 0));
}


// A scene must stay readable after its slide is released: Python code that keeps a
// scene and drops the slide is ordinary. The scene used to hold a raw pointer to a
// page the slide's image owned, so this read dereferenced freed memory.
TEST(GDALDriver, sceneOutlivesItsSlide)
{
    for (const auto& image : {"img_2448x2448_3x8bit_SRC_RGB_ducks.png", "img_2448x2448_3x16bit_SRC_RGB_ducks.tif"}) {
        SCOPED_TRACE(image);
        const std::string path = TestTools::getTestImagePath("gdal", image);
        SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
        std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, "GDAL");
        std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
        cv::Mat expected;
        slideio::SceneInternal::getCVScene(scene)->readBlock(cv::Rect(100, 200, 64, 32), expected);
        slide.reset();
        cv::Mat raster;
        slideio::SceneInternal::getCVScene(scene)->readBlock(cv::Rect(100, 200, 64, 32), raster);
        ASSERT_EQ(expected.size(), raster.size());
        ASSERT_EQ(expected.type(), raster.type());
        EXPECT_EQ(0, cv::norm(expected, raster, cv::NORM_INF));
    }
}

namespace
{
    // An RGB image whose every pixel is a function of its position (and of seed,
    // so two pages of one file differ), so a tile placed in the wrong spot cannot
    // go unnoticed.
    cv::Mat makeRgbImage(int width, int height, int seed = 0)
    {
        cv::Mat image(height, width, CV_8UC3);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                image.at<cv::Vec3b>(y, x) = cv::Vec3b(static_cast<uint8_t>(x + seed),
                                                      static_cast<uint8_t>(y),
                                                      static_cast<uint8_t>((x / 7 + y / 5) * 13 + seed));
            }
        }
        return image;
    }

    // Writes image as one tiled, uncompressed RGB directory. The padding past the
    // right and bottom edges is padValue rather than zero, so a read that lets it
    // bleed into the page shows it.
    void writeTiledRgbDirectory(libtiff::TIFF* tiff, const cv::Mat& image, int tileSize, bool planar,
                                uint8_t padValue)
    {
        const int width = image.cols;
        const int height = image.rows;
        libtiff::TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, width);
        libtiff::TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, height);
        libtiff::TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, 3);
        libtiff::TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, 8);
        libtiff::TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
        libtiff::TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, planar ? PLANARCONFIG_SEPARATE : PLANARCONFIG_CONTIG);
        libtiff::TIFFSetField(tiff, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
        libtiff::TIFFSetField(tiff, TIFFTAG_TILEWIDTH, tileSize);
        libtiff::TIFFSetField(tiff, TIFFTAG_TILELENGTH, tileSize);
        std::vector<cv::Mat> planes;
        if (planar) {
            cv::split(image, planes);
        } else {
            planes.push_back(image);
        }
        for (int plane = 0; plane < static_cast<int>(planes.size()); ++plane) {
            const cv::Mat& source = planes[plane];
            cv::Mat tile(tileSize, tileSize, source.type());
            for (int y = 0; y < height; y += tileSize) {
                for (int x = 0; x < width; x += tileSize) {
                    tile.setTo(cv::Scalar::all(padValue));
                    const cv::Rect part(x, y, std::min(tileSize, width - x), std::min(tileSize, height - y));
                    source(part).copyTo(tile(cv::Rect(0, 0, part.width, part.height)));
                    const auto tileIndex = libtiff::TIFFComputeTile(tiff, x, y, 0,
                                                                    static_cast<uint16_t>(plane));
                    const auto size = static_cast<libtiff::tmsize_t>(tile.total() * tile.elemSize());
                    ASSERT_EQ(size, libtiff::TIFFWriteEncodedTile(tiff, tileIndex, tile.data, size));
                }
            }
        }
    }

    // A tiled RGB tiff whose size is not a multiple of the tile, so the right and
    // bottom tiles are partial.
    void writeTiledRgbTiff(const std::string& path, int width, int height, int tileSize, cv::Mat& image,
                           bool planar = false, uint8_t padValue = 255)
    {
        image = makeRgbImage(width, height);
        libtiff::TIFF* tiff = libtiff::TIFFOpen(path.c_str(), "w");
        ASSERT_TRUE(tiff != nullptr);
        writeTiledRgbDirectory(tiff, image, tileSize, planar, padValue);
        libtiff::TIFFClose(tiff);
    }
}

// A tiled tiff is read tile by tile: the scene reports the file's own tile size,
// so a caller can ask for one tile's worth of pixels without the driver decoding
// the whole page to serve it. A 63803x44997 page used to come back as one
// 63803x44997 "tile", which a viewer then held as one 8.6 GB block.
TEST(GDALDriver, tiledTiffReportsItsTileSize)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    cv::Mat image;
    writeTiledRgbTiff(path, 1000, 700, 256, image);

    slideio::GDALImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(path);
    ASSERT_TRUE(slide != nullptr);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    ASSERT_EQ(1, scene->getNumZoomLevels());
    const slideio::LevelInfo* level = scene->getZoomLevelInfo(0);
    ASSERT_TRUE(level != nullptr);
    EXPECT_EQ(1000, level->getSize().width);
    EXPECT_EQ(700, level->getSize().height);
    EXPECT_EQ(256, level->getTileSize().width);
    EXPECT_EQ(256, level->getTileSize().height);
}

// Reading tile by tile must return exactly the pixels of the page: regions inside
// one tile, across tile seams, and over the partial edge tiles.
TEST(GDALDriver, tiledTiffBlockReadsMatchThePage)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    cv::Mat image;
    writeTiledRgbTiff(path, 1000, 700, 256, image);

    slideio::GDALImageDriver driver;
    std::shared_ptr<slideio::CVScene> scene = driver.openFile(path)->getScene(0);
    const std::vector<cv::Rect> rects = {
        {10, 20, 100, 50},      // inside the first tile
        {200, 230, 120, 90},    // across four tiles
        {900, 600, 100, 100},   // the partial bottom-right tile
        {0, 0, 1000, 700},      // the whole page
    };
    for (const cv::Rect& rect : rects) {
        SCOPED_TRACE(::testing::Message() << rect);
        cv::Mat block;
        scene->readBlock(rect, block);
        ASSERT_EQ(rect.size(), block.size());
        ASSERT_EQ(CV_8UC3, block.type());
        EXPECT_EQ(0, cv::norm(image(rect), block, cv::NORM_INF));
    }
    // A single channel, picked out of the interleaved tile.
    cv::Mat green;
    scene->readBlockChannels({200, 230, 120, 90}, {1}, green);
    cv::Mat expectedGreen;
    cv::extractChannel(image({200, 230, 120, 90}), expectedGreen, 1);
    ASSERT_EQ(CV_8UC1, green.type());
    EXPECT_EQ(0, cv::norm(expectedGreen, green, cv::NORM_INF));
}

// A downscaled read is composed from per-tile linear resizes, so it may differ from
// one linear resize of the whole region -- which is what reading the whole page
// returned -- by interpolation at the seams, never by more.
TEST(GDALDriver, tiledTiffResampledReadMatchesThePage)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    cv::Mat image;
    writeTiledRgbTiff(path, 1000, 700, 256, image);

    slideio::GDALImageDriver driver;
    std::shared_ptr<slideio::CVScene> scene = driver.openFile(path)->getScene(0);
    const cv::Rect rect(0, 0, 1000, 700);
    const cv::Size size(250, 175);
    cv::Mat block;
    scene->readResampledBlock(rect, size, block);
    ASSERT_EQ(size, block.size());
    cv::Mat expected;
    cv::resize(image(rect), expected, size, 0, 0, cv::INTER_LINEAR);
    cv::Mat diff;
    cv::absdiff(expected, block, diff);
    cv::Scalar meanDiff = cv::mean(diff);
    for (int channel = 0; channel < 3; ++channel) {
        EXPECT_LT(meanDiff[channel], 2.0) << "channel " << channel;
    }
}

// Reading over the partial edge tiles at a scale other than 1 must not interpolate
// the encoder's padding into the page. The page edge replicates its last pixel, as
// a resize of the region itself does, so the last row and column agree with that
// resize. (The first ones need not: there the tile read sees the real neighbours
// outside the region, which a resize of the region alone replicates instead.)
TEST(GDALDriver, tiledTiffScaledEdgeReadsKeepPaddingOut)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    cv::Mat image;
    writeTiledRgbTiff(path, 1000, 700, 256, image);

    slideio::GDALImageDriver driver;
    std::shared_ptr<slideio::CVScene> scene = driver.openFile(path)->getScene(0);

    // An upscale of a region inside the partial bottom-right tile.
    const cv::Rect rect(900, 600, 100, 100);
    const cv::Size size(200, 200);
    cv::Mat block;
    scene->readResampledBlock(rect, size, block);
    ASSERT_EQ(size, block.size());
    cv::Mat expected;
    cv::resize(image(rect), expected, size, 0, 0, cv::INTER_LINEAR);
    const cv::Rect lastColumn(size.width - 1, 2, 1, size.height - 2);
    const cv::Rect lastRow(2, size.height - 1, size.width - 2, 1);
    EXPECT_LE(cv::norm(expected(lastColumn), block(lastColumn), cv::NORM_INF), 1.);
    EXPECT_LE(cv::norm(expected(lastRow), block(lastRow), cv::NORM_INF), 1.);

    // A non-integer downscale of the whole page. The second channel is y alone,
    // so it does not change from column to column, and the first is x alone, so it
    // does not change from row to row: the last column must repeat the one before
    // it in the second channel, and the last row the one above it in the first.
    // Padding of 255 interpolated into the edge is the only thing that could make
    // them differ.
    const cv::Size small(333, 233);
    cv::Mat smallBlock;
    scene->readResampledBlock({0, 0, 1000, 700}, small, smallBlock);
    ASSERT_EQ(small, smallBlock.size());
    cv::Mat lastY, previousY, lastX, previousX;
    cv::extractChannel(smallBlock.col(small.width - 1), lastY, 1);
    cv::extractChannel(smallBlock.col(small.width - 2), previousY, 1);
    cv::extractChannel(smallBlock.row(small.height - 1), lastX, 0);
    cv::extractChannel(smallBlock.row(small.height - 2), previousX, 0);
    EXPECT_LE(cv::norm(lastY, previousY, cv::NORM_INF), 1.) << "last column";
    EXPECT_LE(cv::norm(lastX, previousX, cv::NORM_INF), 1.) << "last row";
}

// A planar tiled tiff reads one plane per channel, so reading every channel must
// not collapse to reading none.
TEST(GDALDriver, planarTiledTiffReadsAllChannels)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    cv::Mat image;
    writeTiledRgbTiff(path, 1000, 700, 256, image, true);

    slideio::GDALImageDriver driver;
    std::shared_ptr<slideio::CVScene> scene = driver.openFile(path)->getScene(0);
    const cv::Rect rect(200, 230, 800, 470);
    cv::Mat block;
    scene->readBlock(rect, block);
    ASSERT_EQ(rect.size(), block.size());
    ASSERT_EQ(CV_8UC3, block.type());
    EXPECT_EQ(0, cv::norm(image(rect), block, cv::NORM_INF));

    cv::Mat blue;
    scene->readBlockChannels(rect, {2}, blue);
    cv::Mat expectedBlue;
    cv::extractChannel(image(rect), expectedBlue, 2);
    EXPECT_EQ(0, cv::norm(expectedBlue, blue, cv::NORM_INF));
}

// The pages of one file decode through one libtiff handle, so the scenes of one
// file must serialise their reads together: reading two pages on two threads
// must still return each page's own pixels.
TEST(GDALDriver, twoPagesOfOneTiffReadConcurrently)
{
    slideio::TempFile temp("tif");
    const std::string path = temp.getPath().string();
    const cv::Mat page0 = makeRgbImage(600, 500, 0);
    const cv::Mat page1 = makeRgbImage(600, 500, 100);
    {
        libtiff::TIFF* tiff = libtiff::TIFFOpen(path.c_str(), "w");
        ASSERT_TRUE(tiff != nullptr);
        writeTiledRgbDirectory(tiff, page0, 64, false, 255);
        libtiff::TIFFWriteDirectory(tiff);
        writeTiledRgbDirectory(tiff, page1, 64, false, 255);
        libtiff::TIFFClose(tiff);
    }

    slideio::GDALImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(path);
    ASSERT_EQ(2, slide->getNumScenes());
    const std::shared_ptr<slideio::CVScene> scenes[] = {slide->getScene(0), slide->getScene(1)};
    const cv::Mat* pages[] = {&page0, &page1};
    std::atomic<int> failures{0};
    auto reader = [&](int index) {
        try {
            for (int iteration = 0; iteration < 20; ++iteration) {
                cv::Mat block;
                const cv::Rect rect(0, 0, 600, 500);
                scenes[index]->readBlock(rect, block);
                if (block.size() != rect.size() || cv::norm((*pages[index])(rect), block, cv::NORM_INF) != 0) {
                    ++failures;
                }
            }
        }
        catch (...) {
            ++failures;
        }
    };
    std::thread first(reader, 0);
    std::thread second(reader, 1);
    first.join();
    second.join();
    EXPECT_EQ(0, failures.load());
}
