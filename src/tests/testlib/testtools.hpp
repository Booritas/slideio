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
    static void compareRasters(cv::Mat& raster1, cv::Mat& raster2);
    static void showRaster(cv::Mat& raster);
    static void showRasters(cv::Mat& raster1, cv::Mat& raster2);
    static void writePNG(cv::Mat raster, const std::string& filePath);
    static void readPNG(const std::string& filePath, cv::OutputArray output);
    static void readTiffDirectory(const std::string& filePath, int dir, cv::OutputArray output);
    static void readTiffDirectories(const std::string& filePath, const std::vector<int>& dirIndices, cv::OutputArray output);
};

