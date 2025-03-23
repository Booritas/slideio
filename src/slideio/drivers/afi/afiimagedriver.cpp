// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/afi/afiimagedriver.hpp"
#include "slideio/drivers/afi/afislide.hpp"


slideio::AFIImageDriver::AFIImageDriver()
{
}

slideio::AFIImageDriver::~AFIImageDriver()
{
}

std::string slideio::AFIImageDriver::getID() const
{
	return std::string("AFI");
}

std::shared_ptr<slideio::CVSlide> slideio::AFIImageDriver::openFile(const std::string& filePath)
{
	return AFISlide::openFile(filePath);
}

std::string slideio::AFIImageDriver::getFileSpecs() const
{
	static std::string pattern("*.afi");
    return pattern;
}
