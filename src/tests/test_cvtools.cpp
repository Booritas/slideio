// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/core/cvtools.hpp"
#include <opencv2/opencv.hpp>


using namespace slideio;

TEST(CVTools, extractSliceFrom3D)
{
    const int width = 20;
    const int height = 10;
    const int depth = 5;

    int dimensions[] = { height, width, depth };
    cv::Mat mat3D(3, dimensions, CV_16SC3);
    for(int slice = 0; slice < depth; ++slice)
    {
        std::vector<cv::Range> ranges = {
            cv::Range(0, height),
            cv::Range(0, width),
            cv::Range(slice, slice+1)
        };
        cv::Mat sliceRaster = mat3D(ranges);
        const short val = (short)(slice*10);
        sliceRaster.setTo(cv::Scalar(val+1, val+2, val+3));
    }
    cv::Mat mat2D;
    CVTools::extractSliceFrom3D(mat3D, 3, mat2D);
    EXPECT_EQ(mat2D.cols, width);
    EXPECT_EQ(mat2D.rows, height);
    EXPECT_EQ(mat2D.channels(), 3);
    EXPECT_EQ(mat2D.type(), CV_16SC3);

    cv::Mat test(height, width, CV_16SC3, cv::Scalar(31, 32, 33));
    bool areEqual = (cv::sum(mat2D != test) == cv::Scalar(0, 0, 0, 0));
    EXPECT_TRUE(areEqual);

}

TEST(CVTools, insertSliceInMultidimMatrix)
{
    const int width = 20;
    const int height = 10;
    const int depth = 5;

    int dimensions[] = { height, width, depth };
    cv::Mat mat3D(3, dimensions, CV_16SC3);
    for (int slice = 0; slice < depth; ++slice)
    {
        std::vector<cv::Range> ranges = {
            cv::Range(0, height),
            cv::Range(0, width),
            cv::Range(slice, slice + 1)
        };
        cv::Mat sliceRaster = mat3D(ranges);
        const short val = (short)(slice * 10);
        sliceRaster.setTo(cv::Scalar(val + 1, val + 2, val + 3));
    }
    cv::Mat mat2D(height, width, CV_16SC3, cv::Scalar(1001, 1002, 1003));
    std::vector<int> indices = { 3 };
    CVTools::insertSliceInMultidimMatrix(mat3D, mat2D, indices);

    std::vector<int> dims = {height, width, 1};
    cv::Mat test(dims.size(), dims.data(), CV_16SC3, cv::Scalar(1001, 1002, 1003));

    std::vector<cv::Range> ranges = {
        cv::Range(0, height),
        cv::Range(0, width),
        cv::Range(3, 4)
    };
    cv::Mat slice = mat3D(ranges);
    bool areEqual = (cv::sum(slice != test) == cv::Scalar(0, 0, 0, 0));
    EXPECT_TRUE(areEqual);

}
