// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/converter/convertertools.hpp"


int slideio::ConverterTools::computeNumZoomLevels(int width, int height)
{
    int numZoomLevels = 1;
    int currentWidth(width), currentHeight(height);
    while(currentWidth > 1000 && currentHeight > 1000) {
        currentWidth /= 2;
        currentHeight /= 2;
        numZoomLevels++;
    }
    return numZoomLevels;
}
