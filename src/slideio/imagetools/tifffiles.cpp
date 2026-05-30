// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/imagetools/tifffiles.hpp"
#include "slideio/imagetools/libtiff.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/base/exceptions.hpp"

slideio::TIFFFiles::~TIFFFiles() {
    closeAll();
}

// Returns the URI with any query string ("?...") removed. Presigned S3/HTTP URLs
// carry an "?X-Amz-..." query that is absent from sibling URIs the OME-TIFF driver
// resolves via siblingUri(); comparisons of "same object" must ignore it.
static std::string stripQuery(const std::string& uri) {
    const auto q = uri.find('?');
    return (q == std::string::npos) ? uri : uri.substr(0, q);
}

libtiff::TIFF* slideio::TIFFFiles::getOrOpen(const std::string& filename) {
    auto it = m_openFiles.find(filename);
    if (it != m_openFiles.end()) {
        return it->second.get();
    }
    if (m_mainStream) {
        // Stream-based open: only the main file/URI can be served from the stream.
        // A single-file OME-TIFF self-references by filename; the driver resolves
        // that reference with siblingUri(), which drops the presigned query string,
        // so compare against the main URI with the query string ignored.
        if (stripQuery(filename) == stripQuery(m_mainUri)) {
            libtiff::TIFF* tiff = slideio::TiffTools::openTiffFile(m_mainStream);
            if (!tiff) {
                RAISE_RUNTIME_ERROR << "Failed to open TIFF from stream: " << m_mainUri;
            }
            ++m_openFileCounter;
            int* openFileCounter = &m_openFileCounter;
            m_openFiles[filename] = std::shared_ptr<libtiff::TIFF>(tiff, [openFileCounter](libtiff::TIFF* f) {
                --(*openFileCounter);
                libtiff::TIFFClose(f);
            });
            return tiff;
        }
        // A TiffData referencing a DIFFERENT file than the main stream means a
        // multi-file OME-TIFF. Streaming sibling files is not supported in v1.
        RAISE_RUNTIME_ERROR << "multi-file OME-TIFF over remote streams is not supported in v1; "
                               "use local files. Referenced file: " << filename
                            << " main: " << m_mainUri;
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
