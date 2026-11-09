// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/bif/bifscene.hpp"
#include "slideio/drivers/bif/bifdescription.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/levelinfo.hpp"
#include <algorithm>
#include <cmath>

using namespace slideio;

BIFScene::BIFScene(const std::string& path, const std::string& name, int sceneIndex,
                   std::vector<TiffDirectory> levels, bif::AoiGeometry geometry, bif::Aoi aoi,
                   bif::AcquisitionInfo acq)
    : m_filePath(path),
      m_driverId("BIF"),
      m_name(name),
      m_sceneIndex(sceneIndex),
      m_levelDirectories(std::move(levels)),
      m_geometry(std::move(geometry)),
      m_aoi(std::move(aoi)),
      m_acquisition(std::move(acq))
{
    if (m_levelDirectories.empty()) {
        RAISE_RUNTIME_ERROR << "BIF: scene " << m_name << " has no pyramid levels";
    }
    const TiffDirectory& base = m_levelDirectories.front();
    // A plain Ventana TIFF's synthetic AOI covers whole tiles, so it can overhang the
    // raster; a stitched AOI was checked against the raster by BIFSlide already.
    const cv::Size size = m_geometry.size();
    m_rect = cv::Rect(0, 0, std::min(size.width, base.width), std::min(size.height, base.height));

    std::vector<double> magnifications;
    for (const auto& dir : m_levelDirectories) {
        magnifications.push_back(bif::parseLevelDescription(dir.description).value().magnification);
    }
    m_magnification = m_acquisition.magnification > 0 ? m_acquisition.magnification
                                                       : magnifications.front();
    const double scanRes = m_acquisition.scanResMicrons * 1.e-6;
    m_resolution = Resolution(scanRes, scanRes);
    m_compression = base.slideioCompression;
    m_colorProfile = ColorProfile(base.iccProfile);
    m_rawMetadata = m_aoi.imageInfoXml;
    m_metadataFormat = m_rawMetadata.empty() ? MetadataFormat::None : MetadataFormat::XML;

    for (int level = 0; level < static_cast<int>(m_levelDirectories.size()); ++level) {
        const int d = static_cast<int>(std::lround(magnifications.front() / magnifications[level]));
        m_downsamples.push_back(d);
        const TiffDirectory& dir = m_levelDirectories[level];
        m_levels.emplace_back(level, Size(m_rect.width / d, m_rect.height / d), 1. / d,
                              magnifications[level], Size(dir.tileWidth, dir.tileHeight));
    }
}

void BIFScene::readResampledBlockChannelsEx(const cv::Rect&, const cv::Size&, const std::vector<int>&,
                                            int, int, cv::OutputArray)
{
    RAISE_RUNTIME_ERROR << "BIF: reading pixels is not implemented yet";
}
