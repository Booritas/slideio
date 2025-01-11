// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/base/exceptions.hpp"
#include "slideio/drivers/gdal/gdalslide.hpp"
#include "slideio/drivers/gdal/gdalscene.hpp"

slideio::GDALSlide::GDALSlide(GDALDatasetH ds, const std::string& filePath)
{
	m_scene.reset(new slideio::GDALScene(ds, filePath));
}

slideio::GDALSlide::~GDALSlide()
{
}

int slideio::GDALSlide::getNumScenes() const
{
	return (m_scene==nullptr)?0:1;
}

std::string slideio::GDALSlide::getFilePath() const
{
	if(m_scene!=nullptr)
		return m_scene->getFilePath();
	
	std::string empty_path;
	return empty_path;
}

std::shared_ptr<slideio::CVScene> slideio::GDALSlide::getScene(int index) const
{
	if(index>=getNumScenes()) {
		RAISE_RUNTIME_ERROR << "GDAL driver: invalid scene index";
	}
	return m_scene;
}

