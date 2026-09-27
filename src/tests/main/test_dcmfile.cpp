// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>

#include <opencv2/imgproc.hpp>
#include "slideio/core/exceptions.hpp"
#include "slideio/drivers/dcm/dcmfile.hpp"
#include "tests/testlib/testtools.hpp"

#include "slideio/drivers/dcm/dcmimagedriver.hpp"
#include "slideio/imagetools/imagetools.hpp"

using namespace  slideio;

TEST(DCMFile, init)
{
    DCMImageDriver::initializeDCMTK();

    std::string slidePath = TestTools::getTestImagePath("dcm", "benigns_01/patient0186/0186.LEFT_CC.dcm");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    DCMFile file(slidePath);
    file.init();
    const int width = file.getWidth();
    const int height = file.getHeight();
    const int numSlices = file.getNumSlices();
    const std::string seriesUID = file.getSeriesUID();
    EXPECT_EQ(width, 3984);
    EXPECT_EQ(height, 5528);
    EXPECT_EQ(numSlices, 1);
    EXPECT_EQ(seriesUID, std::string("1.2.276.0.7230010.3.1.4.1787169844.28773.1454574501.602007"));
    EXPECT_EQ(1, file.getNumChannels());
    EXPECT_EQ(file.getSeriesDescription(), "case0377");
    EXPECT_EQ(file.getDataType(), DataType::DT_UInt16);
    EXPECT_FALSE(file.getPlanarConfiguration());
    EXPECT_EQ(file.getPhotointerpretation(), EPhotoInterpetation::PHIN_MONOCHROME2);

}

TEST(DCMFile, initPaletted)
{
    DCMImageDriver::initializeDCMTK();

    std::string slidePath = TestTools::getTestImagePath("dcm", "barre.dev/US-PAL-8-10x-echo");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    DCMFile file(slidePath);
    file.init();
    const int width = file.getWidth();
    const int height = file.getHeight();
    const int numSlices = file.getNumSlices();
    const std::string seriesUID = file.getSeriesUID();
    EXPECT_EQ(width, 600);
    EXPECT_EQ(height, 430);
    EXPECT_EQ(numSlices, 10);
    EXPECT_EQ(3, file.getNumChannels());
    EXPECT_EQ(file.getDataType(), DataType::DT_UInt16);
    EXPECT_EQ(file.getPhotointerpretation(), EPhotoInterpetation::PHIN_PALETTE);

}

TEST(DCMFile, pixelValues)
{
    std::string slidePath = TestTools::getTestImagePath("dcm", "barre.dev/OT-MONO2-8-hip.dcm");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    std::string testPath = TestTools::getTestImagePath("dcm", "barre.dev/OT-MONO2-8-hip.frames/frame0.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testPath);
    DCMFile file(slidePath);
    file.init();
    std::vector<cv::Mat> frames;
    file.readPixelValues(frames);
    ASSERT_FALSE(frames.empty());
    EXPECT_EQ(frames.size(), 1);
    cv::Mat pngImage;
    slideio::ImageTools::readSmallImageRaster(testPath, pngImage);
    double similarity = ImageTools::computeSimilarity(frames[0], pngImage);
    EXPECT_LT(0.99, similarity);
}

TEST(DCMFile, pixelRGB)
{
    std::string slidePath = TestTools::getTestImagePath("dcm", "barre.dev/US-RGB-8-epicard");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    std::string testPath = TestTools::getTestImagePath("dcm", "barre.dev/US-RGB-8-epicard.frames/frame0.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testPath);
    DCMFile file(slidePath);
    file.init();
    std::vector<cv::Mat> frames;
    file.readPixelValues(frames);
    ASSERT_FALSE(frames.empty());
    EXPECT_EQ(frames.size(), 1);
    cv::Mat pngImage;
    slideio::ImageTools::readSmallImageRaster(testPath, pngImage);
    cv::cvtColor(pngImage, pngImage, cv::COLOR_BGR2RGB);
    double similarity = ImageTools::computeSimilarity(frames[0], pngImage);
    EXPECT_EQ(1, similarity);
}

TEST(DCMFile, pixelValuesExtended)
{
    std::string slidePath = TestTools::getTestImagePath("dcm", "barre.dev/MR-MONO2-8-16x-heart");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    std::string testPath1 =  TestTools::getTestImagePath("dcm", "barre.dev/MR-MONO2-8-16x-heart.frames/frame5.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testPath1);
    std::string testPath2 = TestTools::getTestImagePath("dcm", "barre.dev/MR-MONO2-8-16x-heart.frames/frame6.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testPath2);
    DCMFile file(slidePath);
    file.init();
    int fileFrames = file.getNumSlices();
    ASSERT_EQ(16, fileFrames);
    std::vector<cv::Mat> frames;
    file.readPixelValues(frames, 5,2);
    ASSERT_FALSE(frames.empty());
    EXPECT_EQ(frames.size(), 2);
    cv::Mat bmpImage1;
    slideio::ImageTools::readSmallImageRaster(testPath1, bmpImage1);
    double similarity = ImageTools::computeSimilarity(frames[0], bmpImage1);
    EXPECT_EQ(1, similarity);
    cv::Mat bmpImage2;
    slideio::ImageTools::readSmallImageRaster(testPath2, bmpImage2);
    similarity = ImageTools::computeSimilarity(frames[1], bmpImage2);
    EXPECT_EQ(1, similarity);
}

TEST(DCMFile, pixelPaleteExtended)
{
    DCMImageDriver::initializeDCMTK();

    std::string slidePath = TestTools::getTestImagePath("dcm", "barre.dev/US-PAL-8-10x-echo");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    std::string testPath1 = TestTools::getTestImagePath("dcm", "barre.dev/US-PAL-8-10x-echo.frames/frame5.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testPath1);
    std::string testPath2 = TestTools::getTestImagePath("dcm", "barre.dev/US-PAL-8-10x-echo.frames/frame6.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testPath2);
    DCMFile file(slidePath);
    file.init();
    int fileFrames = file.getNumSlices();
    ASSERT_EQ(10, fileFrames);
    std::vector<cv::Mat> frames;
    file.readPixelValues(frames, 5, 2);
    ASSERT_FALSE(frames.empty());
    EXPECT_EQ(frames.size(), 2);
    EXPECT_EQ(3, frames[0].channels());
    cv::Mat bmpImage1;
    slideio::ImageTools::readSmallImageRaster(testPath1, bmpImage1);
    //TestTools::showRaster(frames[0]);
    double similarity = ImageTools::computeSimilarity(frames[0], bmpImage1, true);
    EXPECT_LT(0.92, similarity);
    cv::Mat bmpImage2;
    slideio::ImageTools::readSmallImageRaster(testPath2, bmpImage2);
    //TestTools::showRaster(frames[1]);
    similarity = ImageTools::computeSimilarity(frames[1], bmpImage2, true);
    EXPECT_LT(0.92, similarity);
}


TEST(DCMFile, pixelJpegExtended)
{
    DCMImageDriver::initializeDCMTK();

    std::string slidePath = TestTools::getTestImagePath("dcm", "barre.dev/XA-MONO2-8-12x-catheter");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    std::string testPath1 = TestTools::getTestImagePath("dcm", "barre.dev/XA-MONO2-8-12x-catheter.frames/frame5.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testPath1);
    std::string testPath2 = TestTools::getTestImagePath("dcm", "barre.dev/XA-MONO2-8-12x-catheter.frames/frame6.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testPath2);
    DCMFile file(slidePath);
    file.init();
    int fileFrames = file.getNumSlices();
    ASSERT_EQ(12, fileFrames);
    std::vector<cv::Mat> frames;
    file.readPixelValues(frames, 5, 2);
    ASSERT_FALSE(frames.empty());
    EXPECT_EQ(frames.size(), 2);
    EXPECT_EQ(1, frames[0].channels());
    cv::Mat bmpImage1;
    slideio::ImageTools::readSmallImageRaster(testPath1, bmpImage1);
    double similarity = ImageTools::computeSimilarity(frames[0], bmpImage1);
    EXPECT_LT(0.99, similarity);
    cv::Mat bmpImage2;
    slideio::ImageTools::readSmallImageRaster(testPath2, bmpImage2);
    similarity = ImageTools::computeSimilarity(frames[1], bmpImage2);
    EXPECT_LT(0.99, similarity);
}

TEST(DCMFile, pixelJpegLsValues)
{
    DCMImageDriver::initializeDCMTK();

    std::string slidePath = TestTools::getTestImagePath("dcm", "benigns_01/patient0186/0186.LEFT_MLO.dcm");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    std::string testPath = TestTools::getTestImagePath("dcm", "benigns_01/patient0186/0186.LEFT_MLO.frames/frame0.tif");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testPath);
    DCMFile file(slidePath);
    file.init();
    EXPECT_EQ(4008, file.getWidth());
    EXPECT_EQ(5528, file.getHeight());
    EXPECT_EQ(1, file.getNumChannels());
    std::vector<cv::Mat> frames;
    file.readPixelValues(frames);
    ASSERT_FALSE(frames.empty());
    EXPECT_EQ(frames.size(), 1);

    frames[0].convertTo(frames[0], CV_MAKE_TYPE(CV_8U, 1));

    cv::Mat tiffImage;
    slideio::ImageTools::readSmallImageRaster(testPath, tiffImage);
    double similarity = ImageTools::computeSimilarity(frames[0], tiffImage);
    EXPECT_LT(0.99, similarity);
}

TEST(DCMFile, getMetadata)
{
    std::string slidePath = TestTools::getTestImagePath("dcm", "barre.dev/US-PAL-8-10x-echo");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    DCMFile file(slidePath);
    file.init();
    std::string metadata = file.getMetadata();
    ASSERT_LT(2, metadata.length());
    EXPECT_EQ('{', metadata.front());
    EXPECT_EQ('}', metadata.back());

}

TEST(DCMFile, isDicomDirFile)
{
    std::string filePath = TestTools::getTestImagePath("dcm", "barre.dev/US-PAL-8-10x-echo");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    bool res = DCMFile::isDicomDirFile(filePath);
    EXPECT_FALSE(res);
    filePath = TestTools::getTestImagePath("dcm", "spine_mr/DICOMDIR");
    res = DCMFile::isDicomDirFile(filePath);
    EXPECT_TRUE(res);
}

TEST(DCMFile, channelDataType)
{
    DCMImageDriver::initializeDCMTK();

    std::string slidePath = TestTools::getTestImagePath("dcm", "barre.dev/US-PAL-8-10x-echo");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    DCMFile file(slidePath);
    file.init();
    DataType dt = file.getDataType();
    ASSERT_EQ(dt, DataType::DT_UInt16);
}

TEST(DCMFile, pixelValuesCTMono)
{
    DCMImageDriver::initializeDCMTK();

    std::string slidePath = TestTools::getTestImagePath("dcm", "barre.dev/CT-MONO2-12-lomb-an2");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    std::string testPath = TestTools::getTestImagePath("dcm", "barre.dev/CT-MONO2-12-lomb-an2.frames/frame0.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testPath);
    DCMFile file(slidePath);
    file.init();
    EXPECT_EQ(512, file.getWidth());
    EXPECT_EQ(512, file.getHeight());
    EXPECT_EQ(1, file.getNumChannels());
    EXPECT_EQ(DataType::DT_UInt16, file.getDataType());
    std::vector<cv::Mat> frames;
    file.readPixelValues(frames);
    ASSERT_FALSE(frames.empty());
    EXPECT_EQ(frames.size(), 1);

    double min, max;
    cv::minMaxLoc(frames[0], &min, &max);
    double range = max - min;
    double alpha = 255. / range;
    double beta = -(min * alpha);

    frames[0].convertTo(frames[0], CV_MAKE_TYPE(CV_8U, 1), alpha, beta);

    cv::Mat testImage;
    slideio::ImageTools::readSmallImageRaster(testPath, testImage);
    double similarity = ImageTools::computeSimilarity(frames[0], testImage);
    EXPECT_LT(0.99, similarity);
}

TEST(DCMFile, pixelValues12AllocatedBits)
{
    DCMImageDriver::initializeDCMTK();

    std::string slidePath = TestTools::getTestImagePath("dcm", "barre.dev/MR-MONO2-12-angio-an1");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    std::string testPath = TestTools::getTestImagePath("dcm", "barre.dev/MR-MONO2-12-angio-an1.frames/frame0.tif");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testPath);
    DCMFile file(slidePath);
    file.init();
    EXPECT_EQ(256, file.getWidth());
    EXPECT_EQ(256, file.getHeight());
    EXPECT_EQ(1, file.getNumChannels());
    EXPECT_EQ(DataType::DT_UInt16, file.getDataType());
    std::vector<cv::Mat> frames;
    file.readPixelValues(frames);
    ASSERT_FALSE(frames.empty());
    EXPECT_EQ(frames.size(), 1);

    cv::Mat testImage;
	ImageTools::readSmallImageRaster(testPath, testImage);
    double similarity = ImageTools::computeSimilarity(frames[0], testImage);
    EXPECT_LT(0.98, similarity);
}


TEST(DCMFile, isWSIFile)
{
    std::string filePath = TestTools::getTestImagePath("dcm", "barre.dev/MultiFrame/MR-MONO2-8-16x-heart");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    bool res = DCMFile::isWSIFile(filePath);
    EXPECT_FALSE(res);
    filePath = TestTools::getTestImagePath("dcm", "private/H01EBB50P-24777/H01EBB50P-24777_level-0.dcm");
    res = DCMFile::isWSIFile(filePath);
    EXPECT_TRUE(res);
}

TEST(DCMFile, WSIFileAttributes) {
    std::string filePath = TestTools::getTestImagePath("dcm", "private/H01EBB50P-24777/H01EBB50P-24777_level-0.dcm");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    DCMFile file(filePath);
    file.init();
    EXPECT_TRUE(file.isWSIFile());
    EXPECT_EQ(77550, file.getNumFrames());
    EXPECT_EQ(72192, file.getWidth());
    EXPECT_EQ(70400, file.getHeight());
    EXPECT_EQ(Compression::Jpeg, file.getCompression());
    EXPECT_EQ(0, file.getMagnification());
    EXPECT_EQ(3,file.getNumChannels());
    EXPECT_EQ(1, file.getNumSlices());
    EXPECT_EQ(DataType::DT_Byte, file.getDataType());
    EXPECT_EQ(Resolution(0.,0.), file.getResolution());
}

TEST(DCMFile, getTileRect) {
    std::string filePath = TestTools::getTestImagePath("dcm", "private/H01EBB50P-24777/H01EBB50P-24777_level-0.dcm");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    DCMFile file(filePath);
    file.init();
    const int width = 72192;
    const int height = 70400;
    EXPECT_EQ(width, file.getWidth());
    EXPECT_EQ(height, file.getHeight());

    const cv::Size tileSize(256, 256);
    const int numTilesX = (width -1) / tileSize.width + 1;
    const int numTilesY = (height - 1) / tileSize.height + 1;
    EXPECT_TRUE(file.isWSIFile());

    cv::Rect tileRect;
    EXPECT_TRUE(file.getTileRect(0, tileRect));
    EXPECT_EQ(cv::Rect(0,0,tileSize.width, tileSize.height), tileRect);

    EXPECT_TRUE(file.getTileRect(1, tileRect));
    EXPECT_EQ(cv::Rect(tileSize.width, 0, tileSize.width, tileSize.height), tileRect);

    EXPECT_TRUE(file.getTileRect(numTilesX, tileRect));
    EXPECT_EQ(cv::Rect(0, tileSize.height, tileSize.width, tileSize.height), tileRect);

    EXPECT_THROW(file.getTileRect(numTilesX*numTilesY, tileRect), slideio::RuntimeError);
}

TEST(DCMFile, readFrame) {
    DCMImageDriver::initializeDCMTK();

    std::string filePath = TestTools::getTestImagePath("dcm", "private/H01EBB50P-24777/H01EBB50P-24777_level-0.dcm");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    std::string testFilePath = TestTools::getTestImagePath("dcm", "private/H01EBB50P-24777.tile.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath);
    DCMFile file(filePath);
    file.init();
    const int width = 72192;
    const int height = 70400;
    EXPECT_EQ(width, file.getWidth());
    EXPECT_EQ(height, file.getHeight());

    const cv::Size tileSize(256, 256);
    const int numTilesX = (width - 1) / tileSize.width + 1;
    const int numTilesY = (height - 1) / tileSize.height + 1;
    EXPECT_TRUE(file.isWSIFile());

    cv::Mat tileRaster;
    file.readFrame(2850, tileRaster);
    EXPECT_EQ(tileSize.width, tileRaster.cols);
    EXPECT_EQ(tileSize.height, tileRaster.rows);
    //TestTools::writePNG(tileRaster, testFilePath);
    cv::Mat testImage;
    TestTools::readPNG(testFilePath, testImage);
	double score = ImageTools::computeSimilarity2(testImage, tileRaster);
	EXPECT_GT(score, 0.99);
    //TestTools::showRasters(testImage, tileRaster);

}

TEST(DCMFile, readFrame2) {
    DCMImageDriver::initializeDCMTK();

    std::string filePath = TestTools::getTestImagePath("dcm", "private/wsi/M01FBC14P-589_level-0.dcm");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    std::string testFilePath = TestTools::getTestImagePath("dcm", "private/wsi/M01FBC14P-589_level-0.tile.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath);
    DCMFile file(filePath);
    file.init();
    const int width = 82432;
    const int height = 103936;
    EXPECT_EQ(width, file.getWidth());
    EXPECT_EQ(height, file.getHeight());

    const cv::Size tileSize(256, 256);
    const int numTilesX = (width - 1) / tileSize.width + 1;
    const int numTilesY = (height - 1) / tileSize.height + 1;
    EXPECT_TRUE(file.isWSIFile());

    cv::Mat tileRaster;
    file.readFrame(file.getNumFrames()/3, tileRaster);
    EXPECT_EQ(tileSize.width, tileRaster.cols);
    EXPECT_EQ(tileSize.height, tileRaster.rows);
    //TestTools::writePNG(tileRaster, testFilePath);
    cv::Mat testImage;
    TestTools::readPNG(testFilePath, testImage);
    double score = ImageTools::computeSimilarity2(testImage, tileRaster);
	EXPECT_GT(score, 0.999);
}

TEST(DCMFile, readJ2K) {
    DCMImageDriver::initializeDCMTK();

    std::string filePath = TestTools::getTestImagePath("dcm", "openmicroscopy.org/CT1_J2KI");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    std::string testFilePath = TestTools::getTestImagePath("dcm", "openmicroscopy.org/CT1_J2KI.tiff");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(testFilePath);
    DCMFile file(filePath);
    file.init();
    EXPECT_EQ(512, file.getWidth());
    EXPECT_EQ(512, file.getHeight());
    EXPECT_EQ(1, file.getNumChannels());
    EXPECT_EQ(DataType::DT_Int16, file.getDataType());
    std::vector<cv::Mat> frames;
    file.readPixelValues(frames,0,1);
    //ImageTools::writeTiffImage(testFilePath, frames[0]);
    cv::Mat testImage;
    ImageTools::readSmallImageRaster(testFilePath, testImage);
    double simScore = ImageTools::computeSimilarity2(frames[0], testImage);
    EXPECT_GT(simScore, 0.999);

    //TestTools::showRasters(testImage, frames[0]);
}


TEST(DCMFile, dicomDateTimeToEpochSeconds)
{
    // DA is YYYYMMDD and TM is HHMMSS.FFFFFF, but the corpus holds plenty of
    // files written before that was settled: barre.dev states "1994.10.16" and
    // "11:25:01", which DICOM's own predecessor allowed and readers still meet.
    ASSERT_TRUE(DCMFile::dicomDateTimeToEpochSeconds("20050927", "154452.000000").has_value());
    EXPECT_NEAR(*DCMFile::dicomDateTimeToEpochSeconds("20050927", "154452.000000"),
                1127835892., 1e-6);
    EXPECT_NEAR(*DCMFile::dicomDateTimeToEpochSeconds("20050927", "154452.570999"),
                1127835892.570999, 1e-6);
    EXPECT_NEAR(*DCMFile::dicomDateTimeToEpochSeconds("1996.12.02", "16:38:46.783000"),
                849544726.783, 1e-6);
    // A time may be truncated after the hour or the minute.
    EXPECT_NEAR(*DCMFile::dicomDateTimeToEpochSeconds("20050927", "1544"),
                1127835840., 1e-6);
    EXPECT_NEAR(*DCMFile::dicomDateTimeToEpochSeconds("20050927", "15"),
                1127833200., 1e-6);
}

TEST(DCMFile, dicomDateTimeToEpochSecondsRejectsWhatItCannotRead)
{
    EXPECT_FALSE(DCMFile::dicomDateTimeToEpochSeconds("", "154452").has_value());
    EXPECT_FALSE(DCMFile::dicomDateTimeToEpochSeconds("20050927", "").has_value());
    EXPECT_FALSE(DCMFile::dicomDateTimeToEpochSeconds("2005092", "154452").has_value());
    EXPECT_FALSE(DCMFile::dicomDateTimeToEpochSeconds("2005-09-27", "154452").has_value());
    EXPECT_FALSE(DCMFile::dicomDateTimeToEpochSeconds("20051327", "154452").has_value());
    EXPECT_FALSE(DCMFile::dicomDateTimeToEpochSeconds("20050927", "254452").has_value());
    EXPECT_FALSE(DCMFile::dicomDateTimeToEpochSeconds("20050927", "abcdef").has_value());
}

TEST(DCMFile, dicomDateTimeToEpochSecondsReadsACombinedDateTime)
{
    // AcquisitionDateTime (0008,002A) states both in one DT value, optionally
    // with a "+ZZXX" offset that is not spelled the way ISO 8601 spells one.
    ASSERT_TRUE(DCMFile::dicomDateTimeToEpochSeconds("20050927154452.570999", "").has_value());
    EXPECT_NEAR(*DCMFile::dicomDateTimeToEpochSeconds("20050927154452.570999", ""),
                1127835892.570999, 1e-6);
    EXPECT_NEAR(*DCMFile::dicomDateTimeToEpochSeconds("20050927174452.570999+0200", ""),
                1127835892.570999, 1e-6);
    EXPECT_NEAR(*DCMFile::dicomDateTimeToEpochSeconds("20050927134452.570999-0200", ""),
                1127835892.570999, 1e-6);
}

TEST(DCMFile, significantBitsAndTimesOfAMultiFileSeries)
{
    DCMImageDriver::initializeDCMTK();
    std::string slidePath = TestTools::getTestImagePath("dcm", "series/series_1/IM-0001-0001.dcm");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(slidePath);
    DCMFile file(slidePath);
    file.init();
    // BitsAllocated is 16, BitsStored 12.
    EXPECT_EQ(file.getBitsStored(), 12);
    EXPECT_EQ(file.getDataType(), DataType::DT_UInt16);
    ASSERT_TRUE(file.getAcquisitionTime().has_value());
    EXPECT_NEAR(*file.getAcquisitionTime(), 1127835892., 1e-6);
    ASSERT_TRUE(file.getContentTime().has_value());
    EXPECT_NEAR(*file.getContentTime(), 1127835892.570999, 1e-6);
}

TEST(DCMFile, dicomDateTimeToEpochSecondsAppliesTheInstanceZoneOffset)
{
    // A DA/TM pair cannot carry an offset of its own. TimezoneOffsetFromUTC
    // (0008,0201) states one for every DA and TM in the instance, and without
    // it such a value names a local time the file does not qualify.
    ASSERT_TRUE(DCMFile::dicomDateTimeToEpochSeconds("20050927", "154452.570999",
                                                     "+0200").has_value());
    EXPECT_NEAR(*DCMFile::dicomDateTimeToEpochSeconds("20050927", "154452.570999", "+0200"),
                1127828692.570999, 1e-6);
    EXPECT_NEAR(*DCMFile::dicomDateTimeToEpochSeconds("20050927", "154452.570999", "-0500"),
                1127853892.570999, 1e-6);
    // An offset the value states itself wins over the instance's.
    EXPECT_NEAR(*DCMFile::dicomDateTimeToEpochSeconds("20050927174452.570999+0200", "", "-0500"),
                1127835892.570999, 1e-6);
    // An unreadable instance offset is refused rather than ignored, for the
    // same reason the value's own is.
    EXPECT_FALSE(DCMFile::dicomDateTimeToEpochSeconds("20050927", "154452", "+2").has_value());
}

TEST(DCMFile, timeFromTagsFallsBackToTheSeparatePair)
{
    // The DT VR allows a date with no time at all, and vendors spell it in ways
    // a reader may not follow. An enhanced object states the separate pair as
    // well, so a DT that cannot be read is no reason to report no time.
    ASSERT_TRUE(DCMFile::timeFromTags("20050927", "20050927", "154452.570999", "").has_value());
    EXPECT_NEAR(*DCMFile::timeFromTags("20050927", "20050927", "154452.570999", ""),
                1127835892.570999, 1e-6);
    EXPECT_NEAR(*DCMFile::timeFromTags("not a date", "20050927", "154452.570999", ""),
                1127835892.570999, 1e-6);
    // A DT that does read is preferred: it is the more specific statement.
    EXPECT_NEAR(*DCMFile::timeFromTags("20050927154452.570999", "20050927", "010101", ""),
                1127835892.570999, 1e-6);
    EXPECT_FALSE(DCMFile::timeFromTags("", "20050927", "", "").has_value());
    EXPECT_FALSE(DCMFile::timeFromTags("", "", "", "").has_value());
}

TEST(DCMFile, timeFromTagsKeepsOneClockForBothHalves)
{
    // The origin comes from a DT that may state an offset; the plane time comes
    // from a DA/TM pair that cannot. Reading the pair as UTC while the DT names
    // +02:00 would put the two two hours apart and make a plane that follows
    // its acquisition by 51 ms look like one that precedes it by nearly two
    // hours -- which the scene would answer by dropping the acquisition time.
    const auto acquired =
        DCMFile::timeFromTags("20221213084022.948608+0200", "", "", "+0200");
    const auto content = DCMFile::timeFromTags("", "20221213", "084023.000000", "+0200");
    ASSERT_TRUE(acquired.has_value());
    ASSERT_TRUE(content.has_value());
    EXPECT_NEAR(*acquired, 1670913622.948608, 1e-6);
    EXPECT_NEAR(*content - *acquired, 0.051392, 1e-6);
}
