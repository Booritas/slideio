
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/imagetools/smalltiffwrapper.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include <nlohmann/json.hpp>

using namespace slideio;
using json = nlohmann::json;

SmallTiffWrapper::SmallTiffPage::SmallTiffPage(SmallTiffWrapper* parent, int pageIndex) : 
        m_parent(parent), m_pageIndex(pageIndex) {
	extractMetadata();
}

Size SmallTiffWrapper::SmallTiffPage::getSize() const {
	const TiffDirectory& dir = m_parent->getDirectory(m_pageIndex);
	return Size{ dir.width, dir.height };
}

DataType SmallTiffWrapper::SmallTiffPage::getDataType() const {
	const TiffDirectory& dir = m_parent->getDirectory(m_pageIndex);
	return dir.dataType;
}

int SmallTiffWrapper::SmallTiffPage::getNumChannels() const {
	const TiffDirectory& dir = m_parent->getDirectory(m_pageIndex);
	return dir.channels;
}

Compression SmallTiffWrapper::SmallTiffPage::getCompression() const {
	const TiffDirectory& dir = m_parent->getDirectory(m_pageIndex);
	return dir.slideioCompression;
}

const std::string& SmallTiffWrapper::SmallTiffPage::getMetadata() const {
	return m_metadata;
}

void SmallTiffWrapper::SmallTiffPage::readRaster(cv::OutputArray raster) {
	const TiffDirectory& dir = m_parent->getDirectory(m_pageIndex);
	if (dir.tiled) {
		TiffTools::readTiledDir(m_parent->getHandle(), dir, raster);
	} else {
		TiffTools::readStripedDir(m_parent->getHandle(), dir, raster);
	}
}

Resolution SmallTiffWrapper::SmallTiffPage::getResolution() const {
	const TiffDirectory& dir = m_parent->getDirectory(m_pageIndex);
	return dir.res;
}

void SmallTiffWrapper::SmallTiffPage::extractMetadata() {
	const TiffDirectory& dir = m_parent->getDirectory(m_pageIndex);
	json mtdObj = json::object();
	mtdObj["width"] = dir.width;
	mtdObj["height"] = dir.height;
	mtdObj["tiled"] = dir.tiled;
	mtdObj["interleaved"] = dir.interleaved;
	if (dir.tiled) {
		mtdObj["tileWidth"] = dir.tileWidth;
		mtdObj["tileHeight"] = dir.tileHeight;
	}
	else {
		mtdObj["rowsPerStrip"] = dir.rowsPerStrip;
		mtdObj["stripSize"] = dir.stripSize;
	}
	mtdObj["channels"] = dir.channels;
	mtdObj["dataType"] = dir.dataType;
	mtdObj["compression"] = compressionToString(dir.slideioCompression);
	m_metadata = mtdObj.dump();
}

SmallTiffWrapper::SmallTiffWrapper(const std::string& filePath) {
	m_pTiff.openTiffFile(filePath, true);
	TiffTools::scanFile(m_pTiff.getHandle(), m_directories);
	m_pages.resize(m_directories.size());
}

int SmallTiffWrapper::getNumPages() const {
	return static_cast<int>(m_directories.size());
}

bool SmallTiffWrapper::isValid() const {
	return m_pTiff.isValid();
}

SmallImagePage* slideio::SmallTiffWrapper::readPage(int pageIndex) {
	if (pageIndex < 0 && pageIndex >= getNumPages()) {
		RAISE_RUNTIME_ERROR << "SmallTiffWrapper: invalid page index: " 
			<< pageIndex << ". Valid range is [0, " << getNumPages() - 1 << "]";
	}
	if (m_pages.size() != m_directories.size()) {
		m_pages.resize(m_directories.size());
	}

	if (!m_pages[pageIndex]) {
		m_pages[pageIndex] = std::make_shared<SmallTiffPage>(this, pageIndex);
	}
	return m_pages[pageIndex].get();
}

const TiffDirectory& SmallTiffWrapper::getDirectory(int dirIndex) {
	return m_directories[dirIndex];
}

libtiff::TIFF* SmallTiffWrapper::getHandle() const {
	return m_pTiff.getHandle();
}
