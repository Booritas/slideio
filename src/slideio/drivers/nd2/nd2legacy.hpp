// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/nd2/nd2_api_def.hpp"
#include "slideio/drivers/nd2/nd2framesource.hpp"
#include "slideio/drivers/nd2/nd2imageinfo.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace slideio
{
    class FileReader;

    namespace nd2
    {
        /// One box of a legacy JPEG 2000 ND2 file (FD 11.1).
        struct SLIDEIO_ND2_EXPORTS Box
        {
            uint64_t offset = 0;       // absolute offset of the box header
            uint64_t dataOffset = 0;   // first byte after the header
            uint64_t dataLength = 0;   // bytes after the header
            std::string type;          // JP2 box type, e.g. "jp2c", "xml "
            std::string limType;       // LIM type from the box map, e.g. "VIMD"; empty when unmapped
        };

        /// The container of a legacy (version 1.0) ND2 file: a JP2 box sequence whose
        /// trailing box map gives every box a LIM type (FD 11). Holds no file handle.
        class SLIDEIO_ND2_EXPORTS LegacyFile
        {
        public:
            /// Walks the boxes from offset 0 to EOF and reads the box map. The walk is the
            /// authority: every map offset must be the start of a walked box. Throws
            /// RuntimeError "ND2: ..." otherwise, or when the boxes do not end at EOF.
            static LegacyFile read(const FileReader& file);
            /// Every box, in file order.
            const std::vector<Box>& boxes() const { return m_boxes; }
            /// The `jp2c` boxes, in file order.
            const std::vector<Box>& codestreams() const { return m_codestreams; }
            /// From `jp2h/ihdr`.
            int width() const { return m_width; }
            int height() const { return m_height; }
            int components() const { return m_components; }
            /// Bits per sample (1..128, from the ihdr bpc byte); signedSamples() tells the sign.
            int bits() const { return m_bits; }
            bool signedSamples() const { return m_signed; }
            /// The data of a box of this file.
            std::vector<uint8_t> readBox(const FileReader& file, const Box& box) const;
            /// The first box with this LIM type (VIMD, VCAL, AIM1, AIMD, TINF, ACAL, ...);
            /// nullptr when there is none.
            const Box* first(const std::string& limType) const;
            /// Every box with this LIM type, in file order.
            std::vector<const Box*> all(const std::string& limType) const;

        private:
            std::vector<Box> m_boxes;
            std::vector<Box> m_codestreams;
            int m_width = 0;
            int m_height = 0;
            int m_components = 0;
            int m_bits = 0;
            bool m_signed = false;
        };

        /// The XY count from the `Dimensions:` line of the TINF text (FD 11.3), e.g. 5 for
        /// "Dimensions: T'(81) x XY(5) x lambda(2)"; -1 when the line has no XY(n) term.
        SLIDEIO_ND2_EXPORTS int64_t xyCountFromText(const std::string& textInfo);

        /// Design 4.7. Throws Unsupported for an unsupported file and RuntimeError for a
        /// damaged one.
        SLIDEIO_ND2_EXPORTS ImageInfo buildImageInfo(const LegacyFile& legacy, const FileReader& file,
            const std::string& fileStem);
    }
}
