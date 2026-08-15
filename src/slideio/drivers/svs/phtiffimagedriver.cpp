// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/svs/phtiffimagedriver.hpp"
#include "slideio/drivers/svs/phtiffslide.hpp"
#include "slideio/drivers/svs/phtdescription.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include <exception>

std::string slideio::PHTIFFImageDriver::getID() const
{
	return PHTIFF_DRIVER_ID;
}

std::shared_ptr<slideio::CVSlide> slideio::PHTIFFImageDriver::openFile(const std::string& filePath)
{
	return PHTIFFSlide::openFile(filePath);
}

std::string slideio::PHTIFFImageDriver::getFileSpecs() const
{
	static std::string philipsPattern("*.tif;*.tiff");
	return philipsPattern;
}

bool slideio::PHTIFFImageDriver::canOpenFile(const std::string& filePath) const
{
	if (!ImageDriver::canOpenFile(filePath)) {
		return false;
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
