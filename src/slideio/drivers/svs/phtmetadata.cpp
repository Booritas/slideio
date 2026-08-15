// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/svs/phtmetadata.hpp"
#include "slideio/drivers/svs/phtdescription.hpp"
#include "slideio/base/log.hpp"

#include <algorithm>
#include <tinyxml2.h>

using namespace slideio;
using namespace slideio::phtiff;

namespace
{
    // The pixel spacing an image or a zoom level declares, in the two DICOM_PIXEL_SPACING
    // values philips writes for it. Reads it into `spacing` and warns without raising: a
    // malformed value costs the caller a resolution, not the whole open.
    void phReadSpacing(PHTDescription& philips, const tinyxml2::XMLElement* element, Resolution& spacing) {
        try {
            const std::vector<double> resolutions = philips.getAttributeDoubleList(element, IMAGE_RESOLUTION);
            if (resolutions.size() >= 2) {
                spacing = { resolutions[0], resolutions[1] };
            }
            else {
                SLIDEIO_LOG(WARNING) << "Unexpected DICOM_PIXEL_SPACING value count: " << resolutions.size();
            }
        }
        catch (const std::exception& error) {
            SLIDEIO_LOG(WARNING) << "Cannot extract image resolution: " << error.what();
        }
    }

    // The zoom levels one DPScannedImage declares, ordered by level number. Only used for
    // the whole slide image -- an auxiliary image has no pyramid.
    std::vector<PHTLevelDeclaration> phReadLevels(PHTDescription& philips, const tinyxml2::XMLElement* image) {
        std::vector<PHTLevelDeclaration> levels;
        for (const tinyxml2::XMLElement* level :
             philips.getObjectList(image, LEVEL_SEQUENCE, PIXEL_DATA_REPRESENTATION)) {
            // The level number is what says how much of the slide a level covers, so a
            // level declared without one cannot be placed in the pyramid at all. It is
            // dropped and the rest of the pyramid is kept, rather than the file being
            // refused over one incomplete declaration.
            if (!philips.hasAttribute(level, LEVEL_NUMBER)) {
                SLIDEIO_LOG(WARNING) << "PHTIFFSlide: a zoom level of the philips file declares"
                    " no level number. The level is ignored.";
                continue;
            }
            PHTLevelDeclaration declared;
            declared.number = philips.getAttributeInt(level, LEVEL_NUMBER);
            if (philips.hasAttribute(level, LEVEL_COLUMNS) && philips.hasAttribute(level, LEVEL_ROWS)) {
                declared.declaredSize = {
                    philips.getAttributeInt(level, LEVEL_COLUMNS),
                    philips.getAttributeInt(level, LEVEL_ROWS)
                };
            }
            if (philips.hasAttribute(level, IMAGE_RESOLUTION)) {
                phReadSpacing(philips, level, declared.spacing);
            }
            levels.push_back(declared);
        }
        std::sort(levels.begin(), levels.end(), [](const PHTLevelDeclaration& a, const PHTLevelDeclaration& b) {
            return a.number < b.number;
        });
        return levels;
    }
}

const PHTImageDeclaration* PHTMetadata::wholeSlideImage() const {
    for (const PHTImageDeclaration& image : images) {
        if (image.type == WSI) {
            return &image;
        }
    }
    return nullptr;
}

PHTMetadata slideio::readPHTMetadata(const std::string& description) {
    // Raises if the description cannot be parsed as xml at all -- there is nothing to
    // build a PHTMetadata out of in that case. Every declaration inside a document that
    // does parse is handled individually below, so one incomplete DPScannedImage or
    // PixelDataRepresentation does not cost the caller the rest of the file.
    PHTDescription philips(description);
    PHTMetadata metadata;
    for (const tinyxml2::XMLElement* image :
         philips.getObjectList(philips.getRoot(), SCANNED_IMAGES, SCANNED_IMAGE)) {
        // An image the metadata declares without naming its type cannot be classified,
        // but it says nothing about the other images in the file. The order of the
        // attributes varies between scanners and so does which of them are present, so
        // an incomplete object is skipped rather than allowed to cost the caller the
        // whole slide.
        if (!philips.hasAttribute(image, IMAGE_TYPE)) {
            SLIDEIO_LOG(WARNING) << "PHTIFFTiledScene: a scanned image of the philips file declares"
                " no image type. The image is ignored.";
            continue;
        }
        PHTImageDeclaration declared;
        declared.type = philips.getAttributeText(image, IMAGE_TYPE);
        if (philips.hasAttribute(image, IMAGE_COLUMNS) && philips.hasAttribute(image, IMAGE_ROWS)) {
            declared.size = {
                philips.getAttributeInt(image, IMAGE_COLUMNS),
                philips.getAttributeInt(image, IMAGE_ROWS)
            };
        }
        if (philips.hasAttribute(image, IMAGE_RESOLUTION)) {
            phReadSpacing(philips, image, declared.spacing);
        }
        // Only the whole slide image has a pyramid; an auxiliary image is left with none.
        if (declared.type == WSI) {
            declared.levels = phReadLevels(philips, image);
        }
        metadata.images.push_back(std::move(declared));
    }
    return metadata;
}
