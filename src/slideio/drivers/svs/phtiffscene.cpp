// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/svs/phtiffscene.hpp"

#include "slideio/core/log.hpp"
#include "slideio/drivers/svs/svsdriverids.hpp"
#include "slideio/drivers/svs/svstools.hpp"

using namespace slideio;

PHTIFFTiledScene::PHTIFFTiledScene(const std::string& filePath, libtiff::TIFF* hFile,
    const std::string& name, const std::vector<slideio::TiffDirectory>& dirs,
    const PHTMetadata& metadata)
    : SVSTiledScene(filePath, PHTIFF_DRIVER_ID, hFile, name, dirs), m_metadata(metadata) {
}

std::shared_ptr<PHTIFFTiledScene> PHTIFFTiledScene::create(const std::string& filePath,
    libtiff::TIFF* hFile, const std::string& name,
    const std::vector<slideio::TiffDirectory>& dirs, const PHTMetadata& metadata) {
    std::shared_ptr<PHTIFFTiledScene> scene(new PHTIFFTiledScene(filePath, hFile, name, dirs, metadata));
    scene->initialize();
    return scene;
}

void PHTIFFTiledScene::processImageDescription() {
    if (m_directories.empty()) {
        RAISE_RUNTIME_ERROR <<
            "PHTIFFTiledScene::processImageDescription: directories are empty, cannot construct scene.";
    }
    auto& dir = m_directories.front();
    // The scene describes the tiff directory it reads from, exactly as the aperio branch
    // does. What the philips xml says about the slide belongs to the slide: it is one
    // document covering every image of the file, and 844 KB of it in Philips-2.tiff.
    m_rawMetadata = SVSTools::tiffDirectoryToJson(dir).dump(2);
    m_metadataFormat = MetadataFormat::JSON;

    // Philips names the magnification of every zoom level but the base, whose directory
    // carries the xml metadata instead. Any level gives the magnification of the slide,
    // but the value is stored rounded to six significant digits, so the first level that
    // names one -- the least deeply divided, and therefore the least rounded -- is used.
    for (const TiffDirectory& level : m_directories) {
        m_magnification = SVSTools::extractPhilipsMagnification(level.description);
        if (m_magnification > 0.) {
            break;
        }
    }
    if (m_magnification <= 0.) {
        SLIDEIO_LOG(WARNING) << "PHTIFFTiledScene: no zoom level of the philips file names a"
            " magnification. The magnification of the scene is unknown.";
    }

    // The philips xml is parsed once per open, by PHTIFFSlide::init; this scene only reads
    // the resolution out of the whole slide image's declaration. A file with no whole
    // slide image declared (or none of a recognized shape) leaves the resolution unset,
    // as it did when this scene parsed the xml itself.
    if (const PHTImageDeclaration* wsi = m_metadata.wholeSlideImage()) {
        m_resolution = { wsi->spacing.x * 1.e-3, wsi->spacing.y * 1.e-3 };
    }
}
