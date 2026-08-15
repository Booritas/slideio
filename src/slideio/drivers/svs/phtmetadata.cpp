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

    // Reads an integer attribute, or reports why it could not and leaves the caller's
    // value alone. A value that is present but not an integer is a scanner's mistake in
    // one field; it must not cost the caller the whole slide, which is what letting
    // getAttributeInt raise through readPHTMetadata would do.
    bool phReadInt(PHTDescription& philips, const tinyxml2::XMLElement* element,
                   const PHTDescription::Attribute& attribute, int& value) {
        if (!philips.hasAttribute(element, attribute)) {
            return false;
        }
        try {
            value = philips.getAttributeInt(element, attribute);
            return true;
        }
        catch (const std::exception& error) {
            SLIDEIO_LOG(WARNING) << "PHTIFF: the philips attribute " << attribute.Name
                << " is not readable as a number and is ignored: " << error.what();
            return false;
        }
    }

    // The zoom levels one DPScannedImage declares, ordered by level number. Only used for
    // the whole slide image -- an auxiliary image has no pyramid.
    std::vector<PHTLevelDeclaration> phReadLevels(PHTDescription& philips, const tinyxml2::XMLElement* image) {
        std::vector<PHTLevelDeclaration> levels;
        for (const tinyxml2::XMLElement* level :
             philips.getObjectList(image, LEVEL_SEQUENCE, PIXEL_DATA_REPRESENTATION)) {
            // The level number is what says how much of the slide a level covers, so a
            // level whose number is missing or unreadable cannot be placed in the pyramid
            // at all. It is dropped and the rest of the pyramid is kept, rather than the
            // file being refused over one incomplete declaration.
            PHTLevelDeclaration declared;
            if (!phReadInt(philips, level, LEVEL_NUMBER, declared.number)) {
                SLIDEIO_LOG(WARNING) << "PHTIFF: a zoom level of the philips file declares"
                    " no usable level number. The level is ignored.";
                continue;
            }
            int columns = 0;
            int rows = 0;
            if (phReadInt(philips, level, LEVEL_COLUMNS, columns)
                && phReadInt(philips, level, LEVEL_ROWS, rows)) {
                declared.declaredSize = {columns, rows};
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
            SLIDEIO_LOG(WARNING) << "PHTIFF: a scanned image of the philips file declares"
                " no image type. The image is ignored.";
            continue;
        }
        PHTImageDeclaration declared;
        declared.type = philips.getAttributeText(image, IMAGE_TYPE);
        int columns = 0;
        int rows = 0;
        if (phReadInt(philips, image, IMAGE_COLUMNS, columns)
            && phReadInt(philips, image, IMAGE_ROWS, rows)) {
            declared.size = {columns, rows};
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
