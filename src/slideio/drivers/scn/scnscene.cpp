// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/scn/scnscene.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include <tiffio.h>

using namespace slideio;

SCNScene::SCNScene(const std::string& filePath, const std::string& name):
    m_filePath(filePath),
    m_name(name),
    m_compression(Compression::Unknown),
    m_resolution(0., 0.),
    m_dataType(slideio::DataType::DT_Unknown),
    m_magnification(0.)
{
    m_tiffKeeper = TIFFOpen(filePath.c_str(), "r");
    if (!m_tiffKeeper.isValid())
    {
        throw std::runtime_error(std::string("SCNImageDriver: Cannot open file:") + filePath);
    }
}

SCNScene::~SCNScene()
{
}
