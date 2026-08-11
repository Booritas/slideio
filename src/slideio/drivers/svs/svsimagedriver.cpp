// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/svs/svsimagedriver.hpp"
#include "slideio/drivers/svs/svsslide.hpp"
#include "slideio/base/log.hpp"

slideio::SVSImageDriver::SVSImageDriver(const std::string& driverId)
    : m_driverId(driverId)
{
}

slideio::SVSImageDriver::~SVSImageDriver()
{
}

std::string slideio::SVSImageDriver::getID() const
{
	return m_driverId;
}

std::shared_ptr<slideio::CVSlide> slideio::SVSImageDriver::openFile(const std::string& filePath)
{
	return SVSSlide::openFile(filePath, getID());
}

std::string slideio::SVSImageDriver::getFileSpecs() const
{
	static std::string svsPattern("*.svs");
	static std::string philipsPattern("*.tif;*.tiff");
	if (m_driverId == SVS_DRIVER_ID) {
		return svsPattern;
	}
	return philipsPattern;
}
