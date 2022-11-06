// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "ndpifile.hpp"
#include <boost/filesystem.hpp>

#include "slideio/core/tools/tools.hpp"
#include "slideio/drivers/ndpi/ndpilibtiff.hpp"


slideio::NDPIFile::~NDPIFile()
{
    if(m_tiff) {
        libtiff::TIFFClose(m_tiff);
        m_tiff = nullptr;
    }
}

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

const slideio::NDPITiffDirectory& slideio::NDPIFile::findZoomDirectory(double zoom, int sceneWidth, int dirBegin, int dirEnd)
{
    const auto& directories = m_directories;
    const int dirCount = dirEnd - dirBegin;
    int index = Tools::findZoomLevel(zoom, dirCount, [&directories, sceneWidth, dirBegin](int ind){
        return static_cast<double>(directories[ind+dirBegin].width)/static_cast<double>(sceneWidth);
    });
    return m_directories[index + dirBegin];
}
