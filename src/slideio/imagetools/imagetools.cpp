// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/imagetools/imagetools.hpp"

#include <numeric>

#include "slideio/slideio.hpp"
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <opencv2/imgproc.hpp>


int slideio::ImageTools::dataTypeSize(slideio::DataType dt)
{
    switch(dt)
    {
    case DataType::DT_Byte:
    case DataType::DT_Int8:
        return 1;
    case DataType::DT_UInt16:
    case DataType::DT_Int16:
    case DataType::DT_Float16:
        return 2;
    case DataType::DT_Int32:
    case DataType::DT_Float32:
        return 4;
    case DataType::DT_Float64:
        return 8;
    case DataType::DT_Unknown:
    case DataType::DT_None:
        break;
    }
    throw std::runtime_error(
        (boost::format("Unknown data type: %1%") % (int)dt).str());
}

void slideio::ImageTools::scaleRect(const cv::Rect& srcRect, const cv::Size& newSize, cv::Rect& trgRect)
{
    double scaleX = static_cast<double>(newSize.width) / static_cast<double>(srcRect.width);
    double scaleY = static_cast<double>(newSize.height) / static_cast<double>(srcRect.height);
    trgRect.x = static_cast<int>(std::floor(static_cast<double>(srcRect.x)*scaleX));
    trgRect.y = static_cast<int>(std::floor(static_cast<double>(srcRect.y)*scaleY));
    trgRect.width = newSize.width;
    trgRect.height = newSize.height;
}

void slideio::ImageTools::scaleRect(const cv::Rect& srcRect, double scaleX, double scaleY, cv::Rect& trgRect)
{
    trgRect.x = static_cast<int>(std::floor(static_cast<double>(srcRect.x)*scaleX));
    trgRect.y = static_cast<int>(std::floor(static_cast<double>(srcRect.y)*scaleY));
    int xn = srcRect.x + srcRect.width;
    int yn = srcRect.y + srcRect.height;
    int dxn = static_cast<int>(std::ceil(static_cast<double>(xn)* scaleX));
    int dyn = static_cast<int>(std::ceil(static_cast<double>(yn)* scaleY));
    trgRect.width = dxn - trgRect.x;
    trgRect.height = dyn - trgRect.y;
}

double slideio::ImageTools::computeSimilarity(const cv::Mat& leftM, const cv::Mat& rightM)
{
    double similarity = 0;

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
