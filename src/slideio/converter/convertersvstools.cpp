// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "convertersvstools.hpp"
#include "slideio/base/exceptions.hpp"

void slideio::ConverterSVSTools::checkSVSRequirements(const CVScenePtr& scene)
{
    const DataType dt = scene->getChannelDataType(0);
    if(dt != DataType::DT_Byte) {
        RAISE_RUNTIME_ERROR << "Converter: Only 8bit images are supported now!";
    }
    const int numChannels = scene->getNumChannels();
    if (numChannels != 1 && numChannels !=3) {
        RAISE_RUNTIME_ERROR << "Converter: Only 1 and 3 channels are supported now!";
    }
    for (int channel = 1; channel < numChannels; ++channel) {
        if (dt != scene->getChannelDataType(channel)) {
            RAISE_RUNTIME_ERROR << "Converter: Cannot convert scene with different channel types to SVS!";
        }
    }
}
