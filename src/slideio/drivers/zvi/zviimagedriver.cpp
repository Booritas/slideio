// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/zvi/zviimagedriver.hpp"
#include "slideio/drivers/zvi/zvislide.hpp"
#include <boost/filesystem.hpp>

#include "slideio/core/tools/tools.hpp"

using namespace slideio;

static const std::string filePathPattern = "*.zvi";
static const std::string ID("ZVI");

ZVIImageDriver::ZVIImageDriver() = default;

ZVIImageDriver::~ZVIImageDriver() = default;


std::string ZVIImageDriver::getID() const
{
	return ID;
}

std::shared_ptr<CVSlide> ZVIImageDriver::openFile(const std::string& filePath)
{
    Tools::throwIfPathNotExist(filePath, "ZVIImageDriver::openFile");
    std::shared_ptr<CVSlide> ptr(new ZVISlide(filePath));
    return ptr;
}

std::string ZVIImageDriver::getFileSpecs() const
{
	return filePathPattern;
}
