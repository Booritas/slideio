// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
#include "slideio/core/slide.hpp"
#include "slideio/core/imagedrivermanager.hpp"
#include "slideio/slideio.hpp"

#include <string>

using namespace cv::slideio;

cv::Ptr<Slide> cv::slideio::openSlide(const cv::String& filePath, const cv::String& driver)
{
    return ImageDriverManager::openSlide(filePath, driver);
}

std::vector<cv::String> cv::slideio::getDrivers()
{
    return ImageDriverManager::getDriverIDs();
}
