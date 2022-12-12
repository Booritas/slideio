// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/imagetools/imagetools.hpp"

#include <numeric>

#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include "slideio/imagetools/gdal_lib.hpp"



double slideio::ImageTools::computeSimilarity(const cv::Mat& leftM, const cv::Mat& rightM, bool ignoreTypes)
{
    double similarity = 0;
    const cv::Size leftSize = leftM.size();
    const cv::Size rightSize = rightM.size();
    if(leftSize != rightSize)
    {
        RAISE_RUNTIME_ERROR << "Image sizes for comparison shall be equal. Left image: (" << leftSize.width << "," << leftSize.height
            << "), Right image: (" << rightSize.width << "," << rightSize.height << ")";
    }
    if(leftM.channels() != rightM.channels())
    {
        RAISE_RUNTIME_ERROR << "Number of image channesl for comparison shall be equal. Left image: " << leftM.channels()
            << ". Right image:" << rightM.channels() << ".";
    }
    int dtLeft = leftM.type();
    int dtRight = rightM.type();
    if(!ignoreTypes && dtLeft!=dtRight)
    {
        RAISE_RUNTIME_ERROR << "Image types for comparison shall be equal. Left image: " << dtLeft
            << ". Right image:" << dtRight << ".";
    }
    // convert to 8bit images
    cv::Mat oneChannelLeftM = leftM.reshape(1);
    cv::Mat oneChannelRightM = rightM.reshape(1);
    double minLeft(0), maxLeft(0), minRight(0), maxRight(0);
    cv::minMaxLoc(oneChannelLeftM, &minLeft, &maxLeft);
    cv::minMaxLoc(oneChannelRightM, &minRight, &maxRight);
    double minVal = std::min(minLeft, minRight);
    double maxVal = std::max(maxLeft, maxRight);
    double absVal = maxVal;
    absVal -= minVal;
    double alpha = 255. / absVal;
    double beta = -minVal * alpha;

    cv::Mat left, right;
    leftM.convertTo(left, CV_MAKE_TYPE(CV_8U, left.channels()), alpha, beta);
    rightM.convertTo(right, CV_MAKE_TYPE(CV_8U, right.channels()), alpha, beta);

    cv::Rect rectWindow(0, 0, 30, 30);
    const int width = left.size[1];
    const int height = left.size[0];
    std::vector<double> scores;
    cv::Rect image(0, 0, width, height);
    const int binCount = 10;

    for (int y = 0; y < height; y += rectWindow.height)
    {
        for (int x = 0; x < width; x += rectWindow.width)
        {
            cv::Rect rectWnd2(rectWindow);
            rectWnd2.x = x;
            rectWnd2.y = y;
            cv::Rect rectRoi = rectWnd2 & image;
            cv::Mat leftRoi = left(rectRoi);
            cv::Mat rightRoi = right(rectRoi);
            double score = compareHistograms(leftRoi, rightRoi, binCount);
            scores.push_back(score);
        }
    }
    similarity = std::accumulate(scores.begin(), scores.end(), 0.0) / scores.size();
    return similarity;
}

double slideio::ImageTools::compareHistograms(const cv::Mat& left, const cv::Mat& right, int binCount)
{
    double similarity = 0;

    const int channelCount = left.channels();
    std::vector<int> channels(channelCount);

    std::vector<float*> ranges(channelCount);
    std::vector<int> histSizes(channelCount);
    std::vector<float> rangeVals(channelCount * 2);
    float minVal = 0;
    float maxVal = 256;

    for (int channel = 0; channel < channelCount; ++channel) {
        channels[channel] = channel;
        histSizes[channel] = binCount;
        const int channel2 = channel * 2;
        rangeVals[channel2] = (float)minVal;
        rangeVals[channel2 + 1] = (float)maxVal;
        ranges[channel] = &rangeVals[channel2];
    }

    cv::Mat histLeft, histRight;
    cv::Mat mask;

    cv::calcHist(&left, 1, channels.data(), mask, histLeft,
        channelCount, histSizes.data(), (const float**)ranges.data());

    cv::calcHist(&right, 1, channels.data(), mask, histRight,
        channelCount, histSizes.data(), (const float**)ranges.data());

    similarity = cv::compareHist(histLeft, histRight, cv::HISTCMP_CORREL);

    return similarity;
}