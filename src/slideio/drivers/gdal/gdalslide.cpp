// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/base/exceptions.hpp"
#include "slideio/drivers/gdal/gdalslide.hpp"
#include "slideio/drivers/gdal/gdalscene.hpp"
#include <nlohmann/json.hpp>

#include "slideio/imagetools/fiwrapper.hpp"

using json = nlohmann::json;

slideio::GDALSlide::GDALSlide(const std::string& filePath) : m_filePath(filePath)
{
	m_image = ImageTools::openSmallImage(filePath);
	if (!m_image->isValid()) {
		RAISE_RUNTIME_ERROR << "GDAL driver: cannot open file " << filePath;
	}
	const int numPages = m_image->getNumPages();
	for (int pageIndex = 0; pageIndex < numPages; ++pageIndex) {
		std::shared_ptr<slideio::CVScene> scenePtr(new GDALScene(m_image->readPage(pageIndex), filePath));
		m_scenes.push_back(scenePtr);
	}
	if (numPages == 1) {
		m_rawMetadata = getScene(0)->getRawMetadata();
	}
}

int slideio::GDALSlide::getNumScenes() const
{
	return static_cast<int>(m_scenes.size());
}

std::string slideio::GDALSlide::getFilePath() const
{
	return m_filePath;
}

std::shared_ptr<slideio::CVScene> slideio::GDALSlide::getScene(int index) const
{
	if(index>=getNumScenes()) {
		RAISE_RUNTIME_ERROR << "GDAL driver: invalid scene index";
	}
	return m_scenes[index];
}

slideio::MetadataFormat slideio::GDALSlide::getMetadataFormat() const {
	if (getNumScenes() > 1) {
		return MetadataFormat::None;
	}
    return MetadataFormat::JSON;
}

const std::string& slideio::GDALSlide::getRawMetadata() const {
	return m_rawMetadata;
}
