// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <opencv2/core/mat.hpp>
#include <vector>
#include <stdint.h>

void jpeglibEncode(cv::Mat& raster, std::vector<uint8_t>& encodedStream, int quality);
void jpeglibDecode(const uint8_t* jpg_buffer, size_t jpg_size, cv::OutputArray output);

