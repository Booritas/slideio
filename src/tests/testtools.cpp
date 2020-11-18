#include <gtest/gtest.h>
#include "testtools.hpp"


#include <fstream>
#include <numeric>
#include <boost/filesystem/path.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core/mat.hpp>

static const char* TEST_PATH_VARIABLE = "SLIDEIO_TEST_DATA_PATH";
static const char* PRIV_TEST_PATH_VARIABLE = "SLIDEIO_TEST_DATA_PRIV_PATH";
static const char* TEST_FULL_TEST_PATH_VARIABLE = "SLIDEIO_IMAGE_FOLDER_PATH";


bool TestTools::isPrivateTestEnabled()
{
    const char* var = getenv(PRIV_TEST_PATH_VARIABLE);
    return var != nullptr;
}

bool TestTools::isFullTestEnabled()
{
    const char* var = getenv(TEST_FULL_TEST_PATH_VARIABLE);
    return var != nullptr;
}

std::string TestTools::getTestImageDirectory(bool priv)
{
    const char *varName = priv ? PRIV_TEST_PATH_VARIABLE : TEST_PATH_VARIABLE;
    const char* var = getenv(varName);
    if(var==nullptr)
        throw std::runtime_error(
            std::string("Undefined environment variable: " + std::string(varName)));
    std::string testDirPath(var);
    return testDirPath;
}


std::string TestTools::getTestImagePath(const std::string& subfolder, const std::string& image, bool priv)
{
    std::string imagePath(getTestImageDirectory(priv));
    if(!subfolder.empty())
        imagePath += std::string("/") + subfolder;
    imagePath += std::string("/") +  image;
    return boost::filesystem::path(imagePath).lexically_normal().string();
}

std::string TestTools::getFullTestImagePath(const std::string& subfolder, const std::string& image)
{
    const char* varName = TEST_FULL_TEST_PATH_VARIABLE;
    const char* var = getenv(varName);
    if (var == nullptr)
        throw std::runtime_error(
            std::string("Undefined environment variable: " + std::string(varName)));
    std::string imagePath(var);
    if (!subfolder.empty())
        imagePath += std::string("/") + subfolder;
    imagePath += std::string("/") + image;
    return boost::filesystem::path(imagePath).lexically_normal().string();
}


void TestTools::readRawImage(std::string& path, cv::Mat& image)
{
    std::ifstream is;
    is.open(path, std::ios::binary);
    is.seekg(0, std::ios::end);
    auto length = is.tellg();
    is.seekg(0, std::ios::beg);
    is.read((char*)image.data, image.total() * image.elemSize());
    is.close();
}
double TestTools::computeSimilarity(const cv::Mat& left, const cv::Mat& right)
{
    double similarity = 0;
    cv::Rect rectWindow(0, 0, 30, 30);
    const int width = left.size[1];
    const int height = left.size[0];
    std::vector<double> scores;
    cv::Rect image(0, 0, width, height);
    const int binCount = 10;

    for(int y=0; y<height; y+=rectWindow.height)
    {
        for (int x = 0; x < width; x+=rectWindow.width)
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

double TestTools::compareHistograms(const cv::Mat& leftM, const cv::Mat& rightM, int binCount)
{
    double similarity = 0;
    cv::Mat left, right;
    leftM.convertTo(left, CV_MAKE_TYPE(CV_32F, left.channels()));
    rightM.convertTo(right, CV_MAKE_TYPE(CV_32F, right.channels()));

    const int channelCount = left.channels();
    std::vector<int> channels(channelCount);
    double minLeft(0), maxLeft(0), minRight(0), maxRight(0);
    cv::Mat oneChannelLeft = left.reshape(1);
    cv::Mat oneChannelRight = right.reshape(1);
    cv::minMaxLoc(oneChannelLeft, &minLeft, &maxLeft);
    cv::minMaxLoc(oneChannelRight, &minRight, &maxRight);
    std::vector<int> histSizes(channelCount);
    std::vector<float> rangeVals(channelCount * 2);
    double minVal = std::min(minLeft, minRight);
    double maxVal = std::max(maxLeft, maxRight);
    if (minVal == maxVal) {
        maxVal *= 1.01;
    }
    std::vector<float*> ranges(channelCount);

    for( int channel=0; channel<channelCount; ++channel) {
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
double computeSimilarity(const cv::Mat& leftM, const cv::Mat& rightM)
{
    double similarity = 0;
    // convert matrices to 32 float
    cv::Mat left, right;
    leftM.convertTo(left, CV_MAKE_TYPE(CV_32F, left.channels()));
    rightM.convertTo(right, CV_MAKE_TYPE(CV_32F, right.channels()));
    cv::Mat score;
    cv::matchTemplate(left, right, score, cv::TM_SQDIFF_NORMED);
    double minErr(0), maxErr(0);
    cv::minMaxLoc(score, &minErr, &maxErr);
    //cv::Scalar mean, stdDev;
    //cv::meanStdDev(score, mean, stdDev);
    similarity = 1 - maxErr;
    return similarity;
}

double TestTools::computeSimilarity2(const cv::Mat& leftM, const cv::Mat& rightM)
{
    double similarity = 0;
    // convert matrices to 32 float
    cv::Mat left, right;
    leftM.convertTo(left, CV_MAKE_TYPE(CV_32F, left.channels()));
    rightM.convertTo(right, CV_MAKE_TYPE(CV_32F, right.channels()));

    double minLeft(0), maxLeft(0), minRight(0), maxRight(0);
    cv::minMaxLoc(left, &minLeft, &maxLeft);
    cv::minMaxLoc(right, &minRight, &maxRight);
    double minVal = std::min(minLeft, minRight);
    double maxVal = std::max(maxLeft, maxRight);
    if(minVal <= 0) {
        minVal -= std::abs(maxVal * 0.01);
        maxVal -= minVal;
        cv::Scalar shift;
        for(int channel=0; channel<left.channels(); ++channel) {
            shift[channel] = -minVal;
        }
        cv::add(left, shift, left);
        cv::add(right, shift, right);
    }

    cv::Mat err;
    cv::absdiff(left, right, err);
    cv::Scalar maxScalar;
    for (int channel = 0; channel < left.channels(); ++channel) {
        maxScalar[channel] = maxVal;
    }
    cv::divide(err, maxScalar, err);
    cv::multiply(err, err, err);
    cv::Scalar sqErrScalar = cv::sum(err);
    double sqErr = 0;
    for (int channel = 0; channel < left.channels(); ++channel) {
        sqErr += sqErrScalar[channel];
    }
    double meanSqErr = sqErr / (left.total()*left.channels());
    similarity = 1 - meanSqErr;
    //double minScore(0), maxScore(0);
    //cv::minMaxLoc(err, &minScore, &maxScore);
    //cv::Scalar mean, stddev;
    //cv::meanStdDev(err, mean, stddev);
    //similarity = std::abs(1. - mean[0]);
    return similarity;
}
