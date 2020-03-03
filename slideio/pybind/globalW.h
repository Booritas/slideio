// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "SlideW.h"

std::shared_ptr<SlideW> openSlideW(const std::string& path, const std::string& driver);
std::vector<cv::String> getDriverIDsW();
