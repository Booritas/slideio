// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/svs/svsimagedriver.hpp"
#include "slideio/drivers/svs/svsslide.hpp"
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>


slideio::SVSImageDriver::SVSImageDriver()
{
}

slideio::SVSImageDriver::~SVSImageDriver()
{
}

std::string slideio::SVSImageDriver::getID() const
{
	return std::string("SVS");
}

std::shared_ptr<slideio::Slide> slideio::SVSImageDriver::openFile(const std::string& filePath)
{
	return SVSSlide::openFile(filePath);
}

std::string slideio::SVSImageDriver::getFileSpecs() const
{
	static std::string pattern("*.svs");
    return pattern;
}
