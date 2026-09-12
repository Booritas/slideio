// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// Hand-built, single-directory little-endian classic TIFF writer, for the ndpi ICC
// colour-profile tests only. No libtiff of either build is involved: this project
// links two different physical libtiff builds into one process (the ndpi driver's own
// extern/ndpi-tiff fork, and the regular libtiff the other drivers and
// slideio-imagetools use -- see CLAUDE.md's "Dependencies" section), and calling one
// build's TIFFSetField against a TIFF* the other build's TIFFOpen created was tried
// here first and corrupted the handle in practice (observed: libtiff reporting
// "Unknown tag 256" for TIFFTAG_IMAGEWIDTH, a baseline tag every libtiff always
// registers) -- exactly the two-copies hazard the project's own build already guards
// against for the shared libraries themselves. Writing the handful of raw bytes a
// one-directory TIFF needs by hand sidesteps that hazard entirely: the ndpi driver's
// own fork is the only libtiff that ever touches the resulting file, and it reads a
// conformant TIFF regardless of which tool produced it.
#pragma once

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace slideio_test
{
    inline void appendU16LE(std::vector<uint8_t>& buf, uint16_t v)
    {
        buf.push_back(static_cast<uint8_t>(v & 0xFF));
        buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    }

    inline void appendU32LE(std::vector<uint8_t>& buf, uint32_t v)
    {
        buf.push_back(static_cast<uint8_t>(v & 0xFF));
        buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    }

    // Writes a minimal 16x16 uncompressed RGB classic TIFF at path, embedding iccBytes
    // as TIFFTAG_ICCPROFILE (34675, type BYTE) when non-empty. Throws std::runtime_error
    // if the file cannot be created.
    inline void writeSyntheticIccTiff(const std::string& path, const std::vector<uint8_t>& iccBytes)
    {
        struct Entry { uint16_t tag; uint16_t type; uint32_t count; uint32_t value; };

        const uint32_t width = 16;
        const uint32_t height = 16;
        const uint32_t samplesPerPixel = 3;
        const uint32_t stripByteCount = width * height * samplesPerPixel;

        std::vector<Entry> entries = {
            {256, 3, 1, width},             // ImageWidth (SHORT)
            {257, 3, 1, height},            // ImageLength (SHORT)
            {258, 3, 1, 8},                 // BitsPerSample (SHORT)
            {259, 3, 1, 1},                 // Compression: none
            {262, 3, 1, 2},                 // PhotometricInterpretation: RGB
            {273, 4, 1, 0},                 // StripOffsets (LONG) -- patched below
            {277, 3, 1, samplesPerPixel},   // SamplesPerPixel
            {278, 3, 1, height},            // RowsPerStrip
            {279, 4, 1, stripByteCount},    // StripByteCounts (LONG)
            {284, 3, 1, 1},                 // PlanarConfiguration: contiguous
        };
        if (!iccBytes.empty()) {
            entries.push_back({34675, 1, static_cast<uint32_t>(iccBytes.size()), 0}); // ICCProfile (BYTE[]) -- patched below
        }

        const uint32_t ifdStart = 8;
        const uint32_t dataStart = ifdStart + 2 + static_cast<uint32_t>(entries.size()) * 12 + 4;
        const uint32_t stripOffset = dataStart;
        const uint32_t iccOffset = stripOffset + stripByteCount;

        for (auto& e : entries) {
            if (e.tag == 273) {
                e.value = stripOffset;
            }
            else if (e.tag == 34675) {
                e.value = iccOffset;
            }
        }

        std::vector<uint8_t> buf;
        buf.push_back('I'); buf.push_back('I');   // little-endian
        appendU16LE(buf, 42);                      // classic TIFF magic
        appendU32LE(buf, ifdStart);                // offset to the (only) IFD

        appendU16LE(buf, static_cast<uint16_t>(entries.size()));
        for (const auto& e : entries) {
            appendU16LE(buf, e.tag);
            appendU16LE(buf, e.type);
            appendU32LE(buf, e.count);
            appendU32LE(buf, e.value);
        }
        appendU32LE(buf, 0); // next IFD offset: none

        buf.resize(stripOffset, 0);
        buf.resize(stripOffset + stripByteCount, 0); // pixel data, all zero
        if (!iccBytes.empty()) {
            buf.insert(buf.end(), iccBytes.begin(), iccBytes.end());
        }

        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) {
            throw std::runtime_error("writeSyntheticIccTiff: failed to create " + path);
        }
        out.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    }
}
