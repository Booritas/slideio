// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/vsi/vsiimagedriver.hpp"
#include "slideio/drivers/vsi/vsislide.hpp"
#include "slideio/core/tools/tools.hpp"



slideio::VSIImageDriver::VSIImageDriver()
{
}

slideio::VSIImageDriver::~VSIImageDriver()
{
}

std::string slideio::VSIImageDriver::getID() const
{
	return std::string("VSI");
}

std::shared_ptr<slideio::CVSlide> slideio::VSIImageDriver::openFile(const std::string& filePath)
{
    Tools::throwIfPathNotExist(filePath,"VSIImageDriver::openFile");
    std::shared_ptr<CVSlide> ptr(new vsi::VSISlide(filePath));
    return ptr;
}

std::string slideio::VSIImageDriver::getFileSpecs() const
{
	static std::string pattern("*.vsi");
    return pattern;
}
