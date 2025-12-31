// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/imagetools/fiwrapper.hpp"



void slideio::ImageTools::readGDALImageSubDataset(const std::string& filePath, int pageIndex, cv::OutputArray output)
{
    auto image = ImageTools::openSmallImage(filePath);
    if (!image->isValid()) {
        RAISE_RUNTIME_ERROR << "Cannot open file: " << filePath;
	}
    const int numPages = image->getNumPages();
    if (pageIndex >= numPages || pageIndex <0) {
        RAISE_RUNTIME_ERROR << "Invalid subdataset index " << pageIndex
			<< " for file " << filePath << ". Number of subdatasets: " << numPages;
    }
    auto page = image->readPage(pageIndex);
	page->readRaster(output);
}

void slideio::ImageTools::readGDALImage(const std::string& filePath, cv::OutputArray output)
{
    readGDALImageSubDataset(filePath, 0, output);
}

void slideio::ImageTools::writeRGBImage(const std::string& path, Compression compression, cv::Mat raster)
{
    FIWrapper::writeRaster(path, compression, raster);
}
