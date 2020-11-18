#include <gtest/gtest.h>
#include <opencv2/highgui.hpp>
#include "testtools.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "opencv2/imgproc.hpp"

TEST(TestTools, computeSimilarityEqual)
{
    cv::Mat left(100, 200, CV_16SC1, cv::Scalar((short)55));
    cv::Mat right(100, 200, CV_16SC1, cv::Scalar((short)55));
    double similarity = TestTools::computeSimilarity(left, right);
    EXPECT_DOUBLE_EQ(similarity, 1.);
}

TEST(TestTools, computeSimilarityDifferent)
{
    cv::Mat left(100, 200, CV_16SC1);
    cv::Mat right(100, 200, CV_16SC1);

    double mean = 0.0;
    double stddev = 500.0 / 3.0; // 99.7% of values will be inside [-500, +500] interval
    cv::randn(left, cv::Scalar(mean), cv::Scalar(stddev));
    cv::randu(right, cv::Scalar(-500), cv::Scalar(500));

    double similarity = TestTools::computeSimilarity(left, right);
    EXPECT_LT(similarity,0.5);
}

TEST(TestTools, computeSimilaritySimilar)
{
    std::string pathPng = TestTools::getTestImagePath("jpeg", "lena_256.png");
    cv::Mat left;
    slideio::ImageTools::readGDALImage(pathPng, left);
    cv::Mat right = left.clone();
    cv::blur(right, right, cv::Size(3, 3));
    double similarity = TestTools::computeSimilarity(left, right);
    EXPECT_GT(similarity, 0.9);
}
