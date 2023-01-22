// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/imagedriver.hpp"
#include "slideio/drivers/gdal/gdalimagedriver.hpp"
#include "slideio/drivers/gdal/gdalscene.hpp"
#include "slideio/drivers/gdal/gdalslide.hpp"
#include <boost/algorithm/string.hpp>
#include "slideio/imagetools/gdal_lib.hpp"
#include <set>


slideio::GDALImageDriver::GDALImageDriver()
{
	GDALAllRegister();
}

slideio::GDALImageDriver::~GDALImageDriver()
{
}

std::string slideio::GDALImageDriver::getID() const
{
	return std::string("GDAL");
}


std::shared_ptr<slideio::CVSlide> slideio::GDALImageDriver::openFile(const std::string& filePath)
{
	GDALDatasetH ds = GDALScene::openFile(filePath);
	std::shared_ptr<slideio::CVSlide> ptr(new GDALSlide(ds, filePath));
	return ptr;
}

std::string slideio::GDALImageDriver::getFileSpecs() const
{
	static std::string pattern("*.png;*.jpeg;*.jpg;*.tif;*.tiff;*.bmp;*.gif;*.gtiff;*.gtif;*.ntif;*.jp2");
    return pattern;
}
