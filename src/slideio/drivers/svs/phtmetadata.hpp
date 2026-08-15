// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/drivers/svs/svs_api_def.hpp"
#include "slideio/base/resolution.hpp"
#include <opencv2/core.hpp>
#include <string>
#include <vector>

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
    };

    // Everything the driver needs from the philips xml, parsed once per open. What only
    // the metadata tree needs is deliberately absent: building that tree needs nearly the
    // whole document, and it is built lazily, so folding it in here would parse 844 KB on
    // every open for callers that never ask for metadata.
    struct SLIDEIO_SVS_EXPORTS PHTMetadata
    {
        std::vector<PHTImageDeclaration> images;
        const PHTImageDeclaration* wholeSlideImage() const;
    };

    // Raises if the description is not parseable philips metadata. An image or level
    // declaration that is incomplete is skipped with a warning, not raised on.
    SLIDEIO_SVS_EXPORTS PHTMetadata readPHTMetadata(const std::string& description);
}
