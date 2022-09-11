// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>

#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "slideio/drivers/dcm/dcmfile.hpp"
#include "testtools.hpp"

#include "slideio/drivers/dcm/dcmimagedriver.hpp"
#include "slideio/imagetools/imagetools.hpp"

using namespace  slideio;

TEST(DCMFile, init)
{
    DCMImageDriver::initializeDCMTK();

    std::string slidePath = TestTools::getTestImagePath("dcm", "benigns_01/patient0186/0186.LEFT_CC.dcm");
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
    std::string testPath = TestTools::getTestImagePath("dcm", "barre.dev/OT-MONO2-8-hip.frames/frame0.png");
    DCMFile file(slidePath);
    file.init();
    std::vector<cv::Mat> frames;
    file.readPixelValues(frames);
    ASSERT_FALSE(frames.empty());
    EXPECT_EQ(frames.size(), 1);
    cv::Mat pngImage;
    slideio::ImageTools::readGDALImage(testPath, pngImage);
    double similarity = ImageTools::computeSimilarity(frames[0], pngImage);
    EXPECT_LT(0.99, similarity);
}

TEST(DCMFile, pixelRGB)
{
    std::string slidePath = TestTools::getTestImagePath("dcm", "barre.dev/US-RGB-8-epicard");
    std::string testPath = TestTools::getTestImagePath("dcm", "barre.dev/US-RGB-8-epicard.frames/frame0.png");
    DCMFile file(slidePath);
    file.init();
    std::vector<cv::Mat> frames;
    file.readPixelValues(frames);
    ASSERT_FALSE(frames.empty());
    EXPECT_EQ(frames.size(), 1);
    cv::Mat pngImage;
    slideio::ImageTools::readGDALImage(testPath, pngImage);
    cv::cvtColor(pngImage, pngImage, cv::COLOR_BGR2RGB);
    double similarity = ImageTools::computeSimilarity(frames[0], pngImage);
    EXPECT_EQ(1, similarity);
}

TEST(DCMFile, pixelValuesExtended)
{
    std::string slidePath = TestTools::getTestImagePath("dcm", "barre.dev/MR-MONO2-8-16x-heart");
    std::string testPath1 =  TestTools::getTestImagePath("dcm", "barre.dev/MR-MONO2-8-16x-heart.frames/frame5.png");
    std::string testPath2 = TestTools::getTestImagePath("dcm", "barre.dev/MR-MONO2-8-16x-heart.frames/frame6.png");
    DCMFile file(slidePath);
    file.init();
    int fileFrames = file.getNumSlices();
    ASSERT_EQ(16, fileFrames);
    std::vector<cv::Mat> frames;
    file.readPixelValues(frames, 5,2);
    ASSERT_FALSE(frames.empty());
    EXPECT_EQ(frames.size(), 2);
    cv::Mat bmpImage1;
    slideio::ImageTools::readGDALImage(testPath1, bmpImage1);
    double similarity = ImageTools::computeSimilarity(frames[0], bmpImage1);
    EXPECT_EQ(1, similarity);
    cv::Mat bmpImage2;
    slideio::ImageTools::readGDALImage(testPath2, bmpImage2);
    similarity = ImageTools::computeSimilarity(frames[1], bmpImage2);
    EXPECT_EQ(1, similarity);
}

TEST(DCMFile, pixelPaleteExtended)
{
    DCMImageDriver::initializeDCMTK();

    std::string slidePath = TestTools::getTestImagePath("dcm", "barre.dev/US-PAL-8-10x-echo");
    std::string testPath1 = TestTools::getTestImagePath("dcm", "barre.dev/US-PAL-8-10x-echo.frames/frame5.png");
    std::string testPath2 = TestTools::getTestImagePath("dcm", "barre.dev/US-PAL-8-10x-echo.frames/frame6.png");
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
    slideio::ImageTools::readGDALImage(testPath1, bmpImage1);
    double similarity = ImageTools::computeSimilarity(frames[0], bmpImage1, true);
    EXPECT_LT(0.92, similarity);
    cv::Mat bmpImage2;
    slideio::ImageTools::readGDALImage(testPath2, bmpImage2);
    similarity = ImageTools::computeSimilarity(frames[1], bmpImage2, true);
    EXPECT_LT(0.92, similarity);
}


TEST(DCMFile, pixelJpegExtended)
{
    DCMImageDriver::initializeDCMTK();

    std::string slidePath = TestTools::getTestImagePath("dcm", "barre.dev/XA-MONO2-8-12x-catheter");
    std::string testPath1 = TestTools::getTestImagePath("dcm", "barre.dev/XA-MONO2-8-12x-catheter.frames/frame5.png");
    std::string testPath2 = TestTools::getTestImagePath("dcm", "barre.dev/XA-MONO2-8-12x-catheter.frames/frame6.png");
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
    slideio::ImageTools::readGDALImage(testPath1, bmpImage1);
    double similarity = ImageTools::computeSimilarity(frames[0], bmpImage1);
    EXPECT_LT(0.99, similarity);
    cv::Mat bmpImage2;
    slideio::ImageTools::readGDALImage(testPath2, bmpImage2);
    similarity = ImageTools::computeSimilarity(frames[1], bmpImage2);
    EXPECT_LT(0.99, similarity);
}

TEST(DCMFile, pixelJpegLsValues)
{
    DCMImageDriver::initializeDCMTK();

    std::string slidePath = TestTools::getTestImagePath("dcm", "benigns_01/patient0186/0186.LEFT_MLO.dcm");
    std::string testPath = TestTools::getTestImagePath("dcm", "benigns_01/patient0186/0186.LEFT_MLO.frames/frame0.tif");
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
    slideio::ImageTools::readGDALImage(testPath, tiffImage);
    double similarity = ImageTools::computeSimilarity(frames[0], tiffImage);
    EXPECT_LT(0.99, similarity);
}

TEST(DCMFile, getMetadata)
{
    std::string slidePath = TestTools::getTestImagePath("dcm", "barre.dev/US-PAL-8-10x-echo");
    DCMFile file(slidePath);
    file.init();
    std::string metadata = file.getMetadata();
    ASSERT_LT(2, metadata.length());
    EXPECT_EQ('{', metadata.front());
    EXPECT_EQ('}', metadata.back());

}

TEST(DCMFile, isDicomDirFile)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() <<
            "Skip the test because full dataset is not enabled";
    }
    std::string filePath = TestTools::getTestImagePath("dcm", "barre.dev/US-PAL-8-10x-echo");
    bool res = DCMFile::isDicomDirFile(filePath);
    EXPECT_FALSE(res);
    filePath = TestTools::getFullTestImagePath("dcm", "spine_mr/DICOMDIR");
    res = DCMFile::isDicomDirFile(filePath);
    EXPECT_TRUE(res);
}

TEST(DCMFile, channelDataType)
{
    DCMImageDriver::initializeDCMTK();

    std::string slidePath = TestTools::getTestImagePath("dcm", "barre.dev/US-PAL-8-10x-echo");
    DCMFile file(slidePath);
    file.init();
    DataType dt = file.getDataType();
    ASSERT_EQ(dt, DataType::DT_UInt16);
}

TEST(DCMFile, pixelValuesCTMono)
{
    DCMImageDriver::initializeDCMTK();

    std::string slidePath = TestTools::getTestImagePath("dcm", "barre.dev/CT-MONO2-12-lomb-an2");
    std::string testPath = TestTools::getTestImagePath("dcm", "barre.dev/CT-MONO2-12-lomb-an2.frames/frame0.png");
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
    slideio::ImageTools::readGDALImage(testPath, testImage);
    double similarity = ImageTools::computeSimilarity(frames[0], testImage);
    EXPECT_LT(0.99, similarity);
}

TEST(DCMFile, pixelValues12AllocatedBits)
{
    DCMImageDriver::initializeDCMTK();

    std::string slidePath = TestTools::getTestImagePath("dcm", "barre.dev/MR-MONO2-12-angio-an1");
    std::string testPath = TestTools::getTestImagePath("dcm", "barre.dev/MR-MONO2-12-angio-an1.frames/frame0.tif");
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

    double min, max;
    cv::minMaxLoc(frames[0], &min, &max);
    double range = max - min;
    double alpha = 255. / range;
    double beta = -(min * alpha);
    
    frames[0].convertTo(frames[0], CV_MAKE_TYPE(CV_8U, 1), alpha, beta);

    cv::Mat testImage;
    slideio::ImageTools::readGDALImage(testPath, testImage);
    double similarity = ImageTools::computeSimilarity(frames[0], testImage);
    EXPECT_LT(0.85, similarity);
}
