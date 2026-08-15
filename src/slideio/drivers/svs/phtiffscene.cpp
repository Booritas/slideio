// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/svs/phtiffscene.hpp"

#include <tinyxml2.h>

#include "slideio/base/log.hpp"
#include "slideio/drivers/svs/phtdescription.hpp"
#include "slideio/drivers/svs/svsimagedriver.hpp"
#include "slideio/drivers/svs/svstools.hpp"

using namespace slideio;
using namespace slideio::phtiff;

PHTIFFTiledScene::PHTIFFTiledScene(const std::string& filePath, libtiff::TIFF* hFile,
    const std::string& name, const std::vector<slideio::TiffDirectory>& dirs)
    : SVSTiledScene(filePath, PHTIFF_DRIVER_ID, hFile, name, dirs) {
}

std::shared_ptr<PHTIFFTiledScene> PHTIFFTiledScene::create(const std::string& filePath,
    libtiff::TIFF* hFile, const std::string& name,
    const std::vector<slideio::TiffDirectory>& dirs) {
    std::shared_ptr<PHTIFFTiledScene> scene(new PHTIFFTiledScene(filePath, hFile, name, dirs));
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

    PHTDescription description(dir.description);

    std::vector<const tinyxml2::XMLElement*> images = description.getObjectList(description.getRoot(), SCANNED_IMAGES, SCANNED_IMAGE);
    for (const tinyxml2::XMLElement* image : images) {
        // An image the metadata declares without naming its type cannot be classified,
        // but it says nothing about the other images in the file. The order of the
        // attributes varies between scanners and so does which of them are present, so
        // an incomplete object is skipped rather than allowed to cost the caller the
        // whole slide.
        if (!description.hasAttribute(image, IMAGE_TYPE)) {
            SLIDEIO_LOG(WARNING) << "PHTIFFTiledScene: a scanned image of the philips file declares"
                " no image type. The image is ignored.";
            continue;
        }
        const std::string imageType = description.getAttributeText(image, IMAGE_TYPE);
        if (imageType == WSI) {
            try {
                std::vector<double> resolutions = description.getAttributeDoubleList(image, IMAGE_RESOLUTION);
                if (resolutions.size() >= 2) {
                    m_resolution = { resolutions[0] * 1.e-3, resolutions[1] * 1.e-3 };
				}
				else {
                    SLIDEIO_LOG(WARNING) << "Unexpected DICOM_PIXEL_SPACING value count: " << resolutions.size();
				}
            } catch (std::exception& e) {
                SLIDEIO_LOG(WARNING) << "Cannot extract image resolution: " << e.what();
            }
        }
    }
}
