// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/lif/liftimestamps.hpp"
#include "slideio/core/log.hpp"
#include <tinyxml2.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

using namespace slideio;

namespace
{
    constexpr uint64_t kTicksPerSecond = 10000000ULL;
    // Seconds from 1601-01-01 (FILETIME epoch) to 1970-01-01.
    constexpr int64_t kEpochShiftSeconds = 11644473600LL;

    bool parseHexWord(const std::string& token, uint64_t& value)
    {
        // FD 8.2: the last eight digits are the low word, the digits before them the
        // high word. Together that is one 64-bit value; more than 16 digits cannot be.
        if (token.empty() || token.size() > 16) {
            return false;
        }
        uint64_t v = 0;
        for (char ch : token) {
            const unsigned char c = static_cast<unsigned char>(ch);
            if (!std::isxdigit(c)) {
                return false;
            }
            const unsigned digit = std::isdigit(c) ? c - '0' : (std::tolower(c) - 'a' + 10);
            v = (v << 4) | digit;
        }
        value = v;
        return true;
    }

    bool parseHexList(const char* text, std::vector<uint64_t>& out)
    {
        std::string token;
        const auto flush = [&]() {
            if (token.empty()) {
                return true;
            }
            uint64_t v = 0;
            if (!parseHexWord(token, v)) {
                return false;
            }
            out.push_back(v);
            token.clear();
            return true;
        };
        for (const char* p = text; *p; ++p) {
            if (std::isspace(static_cast<unsigned char>(*p))) {
                if (!flush()) {
                    return false;
                }
            }
            else {
                token.push_back(*p);
            }
        }
        return flush();
    }

    bool parseDecimalList(const tinyxml2::XMLElement* list, std::vector<uint64_t>& out)
    {
        for (const tinyxml2::XMLElement* s = list->FirstChildElement("TimeStamp"); s;
             s = s->NextSiblingElement("TimeStamp")) {
            int64_t high = 0, low = 0;
            if (s->QueryInt64Attribute("HighInteger", &high) != tinyxml2::XML_SUCCESS ||
                s->QueryInt64Attribute("LowInteger", &low) != tinyxml2::XML_SUCCESS) {
                return false;
            }
            // LowInteger is a signed 32-bit word in files; keep only its bits.
            const uint64_t lowWord = static_cast<uint64_t>(low) & 0xFFFFFFFFULL;
            out.push_back((static_cast<uint64_t>(high) << 32) | lowWord);
        }
        return true;
    }
}

std::vector<uint64_t> lif::parseTimestampTicks(const tinyxml2::XMLElement* image)
{
    std::vector<uint64_t> ticks;
    if (!image) {
        return ticks;
    }
    for (const tinyxml2::XMLElement* list = image->FirstChildElement("TimeStampList"); list;
         list = list->NextSiblingElement("TimeStampList")) {
        std::vector<uint64_t> parsed;
        bool ok = true;
        if (list->FirstChildElement("TimeStamp")) {
            ok = parseDecimalList(list, parsed);
        }
        else if (const char* text = list->GetText()) {
            ok = parseHexList(text, parsed);
        }
        if (!ok) {
            // Timestamps are optional metadata: never fail the image over them.
            SLIDEIO_LOG(WARNING) << "LIF: malformed TimeStampList ignored";
            return {};
        }
        if (!parsed.empty()) {
            return parsed;
        }
    }
    return ticks;
}

lif::PlaneTimes lif::PlaneTimes::build(const std::vector<uint64_t>& ticks, const Layout& layout, int field)
{
    PlaneTimes times;
    const int C = layout.numChannels();
    const int Z = layout.numZ();
    const int T = layout.numT();
    const int M = layout.numFields();
    // Interleaved RGB is one stored plane per (z, t, m), as in Layout::storageIndex.
    const uint64_t stored = static_cast<uint64_t>(layout.interleaved() ? 1 : C)
        * static_cast<uint64_t>(Z) * static_cast<uint64_t>(T) * static_cast<uint64_t>(M);
    if (ticks.size() != stored || field < 0 || field >= M) {
        return times;
    }
    std::vector<uint64_t> own(static_cast<size_t>(C) * Z * T);
    for (int t = 0; t < T; ++t) {
        for (int z = 0; z < Z; ++z) {
            for (int c = 0; c < C; ++c) {
                const int64_t k = layout.storageIndex(c, z, t, field);
                own[(static_cast<size_t>(t) * Z + z) * C + c] = ticks[static_cast<size_t>(k)];
            }
        }
    }
    const uint64_t earliest = *std::min_element(own.begin(), own.end());
    // Integer division floors: the origin is the earliest plane's whole second.
    const uint64_t originTicks = (earliest / kTicksPerSecond) * kTicksPerSecond;
    times.m_acquisition = static_cast<int64_t>(earliest / kTicksPerSecond) - kEpochShiftSeconds;
    times.m_numChannels = C;
    times.m_numZ = Z;
    times.m_numT = T;
    times.m_offsets.reserve(own.size());
    for (uint64_t tick : own) {
        // Integer difference first, then scale: no precision lost on 1.3e17 ticks.
        times.m_offsets.push_back(static_cast<double>(tick - originTicks) / static_cast<double>(kTicksPerSecond));
    }
    times.m_complete = true;
    return times;
}

double lif::PlaneTimes::offset(int t, int c, int z) const
{
    if (!m_complete || t < 0 || t >= m_numT || c < 0 || c >= m_numChannels || z < 0 || z >= m_numZ) {
        return 0;
    }
    return m_offsets[(static_cast<size_t>(t) * m_numZ + z) * m_numChannels + c];
}
