// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "ndpifile.hpp"
#include <boost/filesystem.hpp>
#include "slideio/drivers/ndpi/ndpilibtiff.hpp"


void slideio::NDPIFile::init(const std::string& filePath)
{
    namespace fs = boost::filesystem;
    if (!fs::exists(filePath)) {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: File does not exist::" << filePath;
    }
    m_tiff = libtiff::TIFFOpen(filePath.c_str(), "r");
    if (!m_tiff.isValid())
    {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Cannot open file:" << filePath;
    }
    m_filePath = filePath;
    NDPITiffTools::scanFile(m_tiff, m_directories);
}
