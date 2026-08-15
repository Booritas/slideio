// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

namespace slideio
{
    // The public driver ids of the two formats the svs driver library serves.
    // They live here rather than in svsimagedriver.hpp so that the slide and scene
    // classes, which need only to name an id, do not have to include the driver class
    // to get it -- data depending on the driver points the dependency the wrong way.
    constexpr const char* SVS_DRIVER_ID = "SVS";
    constexpr const char* PHTIFF_DRIVER_ID = "PHTIFF";
}
