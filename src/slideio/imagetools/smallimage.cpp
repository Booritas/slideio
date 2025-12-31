// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/imagetools/smallimage.hpp"
#include "slideio/base/size.hpp"

#include <opencv2/core.hpp>

#include "slideio/base/exceptions.hpp"

using namespace slideio;

void SmallImage::readImageStack(cv::OutputArray raster) {
	int numPages = getNumPages();
	slideio::Size pageSize = {};
	std::vector<cv::Mat> pageRasters(numPages);
	for (int i = 0; i < numPages; ++i) {
		SmallImagePage* page = readPage(i);
		if (!page) {
			RAISE_RUNTIME_ERROR << "SmallImage::readImageStack: invalid image page encountered.";
		}
		Size currentPageSize = page->getSize();
		if (i == 0) {
			pageSize = currentPageSize;
		} else {
		     if (currentPageSize != pageSize) {
				 RAISE_RUNTIME_ERROR << "SmallImage::readImageStack: found pages with different sizes.";
		     }
		}
		page->readRaster(pageRasters[i]);
	}
	if (!pageRasters.empty()) {
		cv::merge(pageRasters, raster);
	}
}
