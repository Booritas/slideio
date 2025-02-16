// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <string>
#include <cstdint>
#include "slideio/core/tools/endian.hpp"

std::u16string slideio::Endian::u16StringLittleToBig(const std::u16string& inLE)
{
    std::u16string outBE;
    outBE.reserve(inLE.size());
    for (char16_t codeUnit : inLE) {
        uint16_t u = static_cast<uint16_t>(codeUnit);
        uint16_t swapped = static_cast<uint16_t>((u << 8) | (u >> 8));
        outBE.push_back(static_cast<char16_t>(swapped));
    }
    return outBE;
}