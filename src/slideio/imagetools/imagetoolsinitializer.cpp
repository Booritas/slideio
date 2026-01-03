// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <FreeImage.h>
class ImageToolsInitializer {
public:
    ImageToolsInitializer() {
        FreeImage_Initialise();
    }
    ~ImageToolsInitializer() {
        FreeImage_DeInitialise();
    }
};

static ImageToolsInitializer initializer;