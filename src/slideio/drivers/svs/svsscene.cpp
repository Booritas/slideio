// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/svs/svsscene.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/imagetools/libtiff.hpp"

using namespace slideio;

SVSScene::SVSScene(const std::string& filePath, const std::string& driverId, const std::string& name):
    m_filePath(filePath),
    m_driverId(driverId),
    m_name(name),
    m_compression(Compression::Unknown),
    m_resolution(0., 0.),
    m_dataType(slideio::DataType::DT_Unknown),
    m_magnification(0.),
    m_sceneIndex(0)
{
}

SVSScene::SVSScene(const std::string& filePath, const std::string& driverId, libtiff::TIFF* hFile, const std::string& name):
    m_filePath(filePath),
	m_driverId(driverId),
    m_name(name),
    m_compression(Compression::Unknown),
    m_resolution(0., 0.),
    m_dataType(slideio::DataType::DT_Unknown),
    m_magnification(0.),
    m_sceneIndex(0)
{
    // hFile was opened by the caller while scanning directories (see SVSSlide::openFile /
    // PHTIFFSlide::init), which then handed its ownership to this constructor. Reads now go
    // through the concrete scene's context pool, whose contexts open their own handles, so this
    // handle is not kept for reading -- it is simply closed here to avoid leaking the descriptor.
    TIFFKeeper closer(hFile);
}

SVSScene::~SVSScene() = default;
