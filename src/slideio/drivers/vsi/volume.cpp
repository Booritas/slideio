// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/vsi/volume.hpp"

using namespace slideio;
using namespace slideio::vsi;

void Volume::setChannelName(int channelIndex, const std::string& name) {
    if (channelIndex >= static_cast<int>(m_channelNames.size())) {
        m_channelNames.resize(channelIndex + 1);
    }
    m_channelNames[channelIndex] = name;
}

std::string Volume::getChannelName(int channelIndex) const {
    if (channelIndex >= static_cast<int>(m_channelNames.size())) {
        return "";
    }
    return m_channelNames[channelIndex];
}
