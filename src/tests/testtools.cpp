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
double TestTools::computeSimilarity(const cv::Mat& leftM, const cv::Mat& rightM)
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

double TestTools::compareHistograms(const cv::Mat& left, const cv::Mat& right, int binCount)
{
    double similarity = 0;

    const int channelCount = left.channels();
    std::vector<int> channels(channelCount);

    std::vector<float*> ranges(channelCount);
    std::vector<int> histSizes(channelCount);
    std::vector<float> rangeVals(channelCount * 2);
    float minVal = 0;
    float maxVal = 256;

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
