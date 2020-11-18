// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <string>
#include <opencv2/core/mat.hpp>

class TestTools
{
public:
	static bool isPrivateTestEnabled();
    static bool isFullTestEnabled();
    static std::string getTestImageDirectory(bool priv=false);
	static std::string getTestImagePath(const std::string& subfolder, const std::string& image, bool priv=false);
    static std::string getFullTestImagePath(const std::string& subfolder, const std::string& image);
    static void readRawImage(std::string& path, cv::Mat& image);
    static double computeSimilarity(const cv::Mat& left, const cv::Mat& right);
    static double compareHistograms(const cv::Mat& leftM, const cv::Mat& rightM, int bins);
    static double computeSimilarity2(const cv::Mat& left, const cv::Mat& right);
};

