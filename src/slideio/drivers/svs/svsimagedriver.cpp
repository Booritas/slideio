// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/svs/svsimagedriver.hpp"
#include "slideio/drivers/svs/svsslide.hpp"
#include "slideio/drivers/svs/phtdescription.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/base/log.hpp"
#include <exception>

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

bool slideio::SVSImageDriver::canOpenFile(const std::string& filePath) const
{
	if (!ImageDriver::canOpenFile(filePath)) {
		return false;
	}
	if (m_driverId != PHTIFF_DRIVER_ID) {
		// The aperio format has an extension of its own.
		return true;
	}
	// *.tif and *.tiff say nothing: gdal reads plain tiff files and the ome-tiff driver
	// reads its own flavour. Only the metadata in the description of the first directory
	// identifies a philips file.
	try {
		TIFFKeeper keeper(filePath);
		if (!keeper.isValid()) {
			return false;
		}
		return PHTDescription::isPhilipsDescription(keeper.readStringTag(TIFFTAG_IMAGEDESCRIPTION));
	}
	catch (const std::exception&) {
		// A missing file or one that is not a tiff at all: not ours, and not an error
		// worth propagating out of format detection.
		return false;
	}
}
