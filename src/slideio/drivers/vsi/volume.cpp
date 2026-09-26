// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/vsi/volume.hpp"
#include "slideio/drivers/vsi/vsitools.hpp"

using namespace slideio;
using namespace slideio::vsi;

void Volume::setTResolution(double raw, const std::string& unit) {
    m_tResolution = 0.;
    if (raw <= 0.0) {
        return;
    }
    if (const auto scale = VSITools::unitToSeconds(unit)) {
        m_tResolution = raw * (*scale);
    }
}

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

void Volume::setChannelColor(int channelIndex, uint8_t r, uint8_t g, uint8_t b) {
    if (channelIndex < 0) {
        return;
    }
    if (channelIndex >= static_cast<int>(m_channelColors.size())) {
        m_channelColors.resize(channelIndex + 1, kNoChannelColor);
    }
    m_channelColors[channelIndex] =
        (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
}

bool Volume::hasChannelColor(int channelIndex) const {
    if (channelIndex < 0 || channelIndex >= static_cast<int>(m_channelColors.size())) {
        return false;
    }
    return m_channelColors[channelIndex] != kNoChannelColor;
}

void Volume::getChannelColor(int channelIndex, uint8_t& r, uint8_t& g, uint8_t& b) const {
    if (!hasChannelColor(channelIndex)) {
        r = g = b = 0;
        return;
    }
    const uint32_t c = m_channelColors[channelIndex];
    r = static_cast<uint8_t>((c >> 16) & 0xFF);
    g = static_cast<uint8_t>((c >> 8) & 0xFF);
    b = static_cast<uint8_t>(c & 0xFF);
}

void Volume::setChannelEmissionWavelength(int channelIndex, double nm) {
    if (channelIndex < 0) {
        return;
    }
    if (channelIndex >= static_cast<int>(m_channelEmissionWavelengths.size())) {
        m_channelEmissionWavelengths.resize(channelIndex + 1, 0.0);
    }
    m_channelEmissionWavelengths[channelIndex] = nm;
}

double Volume::getChannelEmissionWavelength(int channelIndex) const {
    if (channelIndex < 0 || channelIndex >= static_cast<int>(m_channelEmissionWavelengths.size())) {
        return 0.0;
    }
    return m_channelEmissionWavelengths[channelIndex];
}

void Volume::setPlaneTimestamps(const std::vector<double>& raw, const std::string& unit) {
    m_planeTimestamps.clear();
    const auto scale = VSITools::unitToSeconds(unit);
    if (!scale) {
        return;
    }
    m_planeTimestamps.reserve(raw.size());
    for (const double value : raw) {
        m_planeTimestamps.push_back(value * (*scale));
    }
}

int Volume::getPlaneTimestampCount() const {
    return static_cast<int>(m_planeTimestamps.size());
}

double Volume::getPlaneTimestampByIndex(int index) const {
    if (index < 0 || index >= static_cast<int>(m_planeTimestamps.size())) {
        return 0.0;
    }
    return m_planeTimestamps[static_cast<std::size_t>(index)];
}
