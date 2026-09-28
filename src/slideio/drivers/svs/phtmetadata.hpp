// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/drivers/svs/svs_api_def.hpp"
#include "slideio/core/resolution.hpp"
#include <opencv2/core.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace slideio
{
    // One zoom level as the philips metadata declares it. The declared size is optional:
    // the metadata may omit LEVEL_COLUMNS/LEVEL_ROWS, and a zero size means "not stated".
    struct PHTLevelDeclaration
    {
        int number = 0;
        cv::Size declaredSize = {};
        Resolution spacing = {};
    };

    // One DPScannedImage: the whole slide image or an auxiliary one.
    struct PHTImageDeclaration
    {
        std::string type;
        cv::Size size = {};
        Resolution spacing = {};
        std::vector<PHTLevelDeclaration> levels;   // empty for an auxiliary image
        // DICOM_BITS_STORED: how many of the allocated bits carry data. 0 where the
        // image declares none.
        int significantBits = 0;
    };

    // Everything the driver needs from the philips xml, parsed once per open. What only
    // the metadata tree needs is deliberately absent: building that tree needs nearly the
    // whole document, and it is built lazily, so folding it in here would parse 844 KB on
    // every open for callers that never ask for metadata.
    struct SLIDEIO_SVS_EXPORTS PHTMetadata
    {
        std::vector<PHTImageDeclaration> images;
        // DICOM_ACQUISITION_DATETIME of the slide, in seconds since the Unix epoch;
        // 0 where the file states none. It is a root attribute, so it covers every
        // image of the file rather than one of them.
        int64_t acquisitionTime = 0;
        const PHTImageDeclaration* wholeSlideImage() const;
    };

    // Raises if the description is not parseable philips metadata. An image or level
    // declaration that is incomplete is skipped with a warning, not raised on.
    SLIDEIO_SVS_EXPORTS PHTMetadata readPHTMetadata(const std::string& description);
}
