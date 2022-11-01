// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include <string>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/mat.hpp>

namespace slideio
{
	class NDPITestTools
	{
	public:
		static void writePNG(cv::Mat raster, const std::string& filePath);
		static void readPNG(const std::string& filePath, cv::OutputArray output);
	};
}
