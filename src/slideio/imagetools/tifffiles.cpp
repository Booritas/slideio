// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/imagetools/tifffiles.hpp"
#include "slideio/imagetools/libtiff.hpp"
#include "slideio/base/exceptions.hpp"

slideio::TIFFFiles::~TIFFFiles() {
    closeAll();
}

libtiff::TIFF* slideio::TIFFFiles::getOrOpen(const std::string& filename) {
    auto it = m_openFiles.find(filename);
    if (it != m_openFiles.end()) {
        return it->second.get();
    }
    libtiff::TIFF* tiff = libtiff::TIFFOpen(filename.c_str(), "r");
    if(tiff) {
		m_openFileCounter++;
	}
    else {
        RAISE_RUNTIME_ERROR << "Failed to open TIFF file: " << filename;
    }
    // Use custom deleter to ensure TIFFClose is called
	int* openFileCounter = &m_openFileCounter;
    m_openFiles[filename] = std::shared_ptr<libtiff::TIFF>(tiff, [openFileCounter](libtiff::TIFF* f) {
        --(*openFileCounter);
        libtiff::TIFFClose(f);
    });
    return tiff;
}

void slideio::TIFFFiles::close(const std::string& filename) {
    m_openFiles.erase(filename);
}

void slideio::TIFFFiles::closeAll() {
    m_openFiles.clear(); // shared_ptr will call TIFFClose
}
