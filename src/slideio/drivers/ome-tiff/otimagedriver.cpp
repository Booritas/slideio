// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/ome-tiff/otimagedriver.hpp"
#include "slideio/drivers/ome-tiff/otslide.hpp"
#include "slideio/base/log.hpp"

using namespace slideio;
using namespace slideio::ometiff;

OTImageDriver::OTImageDriver()
{
}

OTImageDriver::~OTImageDriver()
{
}

std::string OTImageDriver::getID() const
{
	return std::string("QPTIFF");
}

std::shared_ptr<slideio::CVSlide> OTImageDriver::openFile(const std::string& filePath)
{
	return OTSlide::openFile(filePath);
}

std::string OTImageDriver::getFileSpecs() const
{
	static std::string pattern("*.qptiff");
    return pattern;
}
