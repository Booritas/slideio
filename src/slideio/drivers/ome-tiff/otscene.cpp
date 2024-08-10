// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/ome-tiff/otscene.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/imagetools/libtiff.hpp"

using namespace slideio;
using namespace slideio::ometiff;

OTScene::OTScene(const std::string& filePath, const std::string& name):
    m_filePath(filePath),
    m_name(name),
    m_compression(Compression::Unknown),
    m_resolution(0., 0.),
    m_dataType(slideio::DataType::DT_Unknown),
    m_magnification(0.)
{
}

OTScene::OTScene(const std::string& filePath, libtiff::TIFF* hFile, const std::string& name):
    m_filePath(filePath),
    m_name(name),
    m_compression(Compression::Unknown),
    m_resolution(0., 0.),
    m_dataType(slideio::DataType::DT_Unknown),
    m_magnification(0.),
    m_tiffKeeper(hFile)
{
}

OTScene::~OTScene() = default;

void OTScene::makeSureFileIsOpened()
{
    if (!m_tiffKeeper.isValid()) {
        m_tiffKeeper = TiffTools::openTiffFile(m_filePath);
        if(!m_tiffKeeper.isValid()) {
            throw std::runtime_error(std::string("OTImageDriver: Cannot open file:") + m_filePath);
        }
    }
}

libtiff::TIFF* OTScene::getFileHandle()
{
    makeSureFileIsOpened();
    return m_tiffKeeper;
}
