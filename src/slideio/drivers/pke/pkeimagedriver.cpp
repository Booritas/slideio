// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/pke/pkeimagedriver.hpp"
#include "slideio/drivers/pke/pkeslide.hpp"
#include <boost/filesystem.hpp>
#include "slideio/base/log.hpp"

slideio::PKEImageDriver::PKEImageDriver()
{
}

slideio::PKEImageDriver::~PKEImageDriver()
{
}

std::string slideio::PKEImageDriver::getID() const
{
	return std::string("QPTIFF");
}

std::shared_ptr<slideio::CVSlide> slideio::PKEImageDriver::openFile(const std::string& filePath)
{
	return PKESlide::openFile(filePath);
}

std::string slideio::PKEImageDriver::getFileSpecs() const
{
	static std::string pattern("*.qptiff");
    return pattern;
}
