// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/exceptions.hpp"
#include "slideio/drivers/gdal/gdalslide.hpp"
#include "slideio/drivers/gdal/gdalscene.hpp"
#include "slideio/drivers/gdal/gdaltiffgrouping.hpp"
#include "slideio/drivers/gdal/gdaltiffscene.hpp"
#include <nlohmann/json.hpp>

#include "slideio/imagetools/fiwrapper.hpp"
#include "slideio/imagetools/smalltiffwrapper.hpp"

using json = nlohmann::json;

slideio::GDALSlide::GDALSlide(const std::string& filePath, const std::string& driverId) : m_filePath(filePath)
{
	m_driverId = driverId;
	std::shared_ptr<SmallImage> image = ImageTools::openSmallImage(filePath);
	if (!image->isValid()) {
		RAISE_RUNTIME_ERROR << "GDAL driver: cannot open file " << filePath;
	}
	if (auto* tiff = dynamic_cast<SmallTiffWrapper*>(image.get())) {
		// A tiff: the scan is all the scenes need. They read through handles of
		// their own, so the wrapper's handle is closed before the constructor ends.
		std::vector<TiffDirectory> directories;
		const int numPages = tiff->getNumPages();
		directories.reserve(numPages);
		for (int pageIndex = 0; pageIndex < numPages; ++pageIndex) {
			directories.push_back(tiff->getDirectory(pageIndex));
		}
		image.reset();
		// One pool for the whole file, shared by all its scenes, so the handles the
		// file holds are bounded by the pool rather than growing with the page
		// count. The factory captures the path by value, not a scene or the slide,
		// as SVSTiledScene's does: the pool outlives whichever of them goes first.
		auto contextPool = std::make_shared<ContextPool>(
			[filePath]() { return std::make_unique<GDALReadContext>(filePath); });
		for (auto& group : groupTiffDirectories(directories)) {
			m_scenes.push_back(std::make_shared<GDALTiffScene>(filePath, getDriverId(), std::move(group.levels),
				contextPool));
		}
		if (m_scenes.empty()) {
			RAISE_RUNTIME_ERROR << "GDAL driver: no image found in tiff file " << filePath;
		}
	}
	else {
		// Every scene keeps the image alive; FreeImage locked each page's bitmap
		// when the scene was built, and reads only copy out of it.
		const int numPages = image->getNumPages();
		for (int pageIndex = 0; pageIndex < numPages; ++pageIndex) {
			m_scenes.push_back(std::make_shared<GDALScene>(image, pageIndex, filePath, getDriverId()));
		}
	}
	if (getNumScenes() == 1) {
		m_rawMetadata = getScene(0)->getRawMetadata();
		m_metadataFormat = MetadataFormat::JSON;
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
	if(index<0 || index>=getNumScenes()) {
		RAISE_RUNTIME_ERROR << "GDAL driver: invalid scene index: " << index
			<< ". Total scenes: " << getNumScenes();
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
