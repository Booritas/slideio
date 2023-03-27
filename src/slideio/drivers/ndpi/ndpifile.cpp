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
        SLIDEIO_LOG(INFO) << "Closing file " << m_filePath;
        NDPITiffTools::closeTiffFile(m_tiff);
        m_tiff = nullptr;
    }
}

void slideio::NDPIFile::init(const std::string& filePath)
{
    SLIDEIO_LOG(INFO) << "Initialization of NDPI TIFF file : " << filePath;

    Tools::throwIfPathNotExist(filePath, "NDPIFile::init");
    SLIDEIO_LOG(INFO) << "Opening of NDPI TIFF file " << filePath;
    m_tiff = NDPITiffTools::openTiffFile(filePath);
    if (!m_tiff.isValid())
    {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: Cannot open file:" << filePath;
    }
    SLIDEIO_LOG(INFO) << "File " << filePath << " is successfully opened";
    m_filePath = filePath;
    NDPITiffTools::scanFile(m_tiff, m_directories);
    SLIDEIO_LOG(INFO) << "File " << filePath << " initialization is complete";
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
