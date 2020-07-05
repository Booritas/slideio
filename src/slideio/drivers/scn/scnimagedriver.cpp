// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/scn/scnimagedriver.hpp"
#include "slideio/drivers/scn/scnslide.hpp"
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>


slideio::SCNImageDriver::SCNImageDriver()
{
}

slideio::SCNImageDriver::~SCNImageDriver()
{
}

std::string slideio::SCNImageDriver::getID() const
{
	return std::string("SCN");
}

std::shared_ptr<slideio::CVSlide> slideio::SCNImageDriver::openFile(const std::string& filePath)
{
    namespace fs = boost::filesystem;
    if (!fs::exists(filePath)) {
        throw std::runtime_error(std::string("CZIImageDriver: File does not exist:") + filePath);
    }
    std::shared_ptr<CVSlide> ptr(new SCNSlide(filePath));
    return ptr;
}

std::string slideio::SCNImageDriver::getFileSpecs() const
{
	static std::string pattern("*.scn");
    return pattern;
}
