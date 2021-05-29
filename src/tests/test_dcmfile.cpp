// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <opencv2/imgcodecs.hpp>

#include "slideio/drivers/dcm/dcmfile.hpp"
#include "testtools.hpp"
#include "slideio/imagetools/imagetools.hpp"

using namespace  slideio;

TEST(DCMFile, init)
{
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

TEST(DCMFile, pixelValues)
{
    std::string slidePath = TestTools::getTestImagePath("dcm", "openmicroscopy/OT-MONO2-8-hip.dcm");
    std::string testPath = TestTools::getTestImagePath("dcm", "openmicroscopy/Test/OT-MONO2-8-hip.bmp");
    DCMFile file(slidePath);
    file.init();
    std::vector<cv::Mat> frames;
    file.readPixelValues(frames);
    ASSERT_FALSE(frames.empty());
    EXPECT_EQ(frames.size(), 1);
    cv::Mat bmpImage = cv::imread(testPath, cv::IMREAD_UNCHANGED);
    double similarity = ImageTools::computeSimilarity(frames[0], bmpImage);
    EXPECT_EQ(1, similarity);
}


TEST(DCMFile, pixelValuesExtended)
{
    std::string slidePath = TestTools::getTestImagePath("dcm", "bare.dev/MR-MONO2-8-16x-heart");
    std::string testPath1 =  TestTools::getTestImagePath("dcm", "bare.dev/MR-MONO2-8-16x-heart.frames/frame5.png");
    std::string testPath2 = TestTools::getTestImagePath("dcm", "bare.dev/MR-MONO2-8-16x-heart.frames/frame6.png");
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