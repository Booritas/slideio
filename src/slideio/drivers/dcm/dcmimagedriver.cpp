// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/dcm/dcmimagedriver.hpp"
#include "slideio/drivers/dcm/dcmslide.hpp"
#include <boost/filesystem.hpp>

using namespace slideio;

static const std::string filePathPattern = "*.dcm";
static const std::string ID("DCM");

DCMImageDriver::DCMImageDriver() = default;

DCMImageDriver::~DCMImageDriver() = default;


std::string DCMImageDriver::getID() const
{
	return ID;
}

std::shared_ptr<CVSlide> DCMImageDriver::openFile(const std::string& filePath)
{
	return nullptr;
}

std::string DCMImageDriver::getFileSpecs() const
{
	return filePathPattern;
}
