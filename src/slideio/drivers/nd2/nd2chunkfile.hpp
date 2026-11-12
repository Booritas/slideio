// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/nd2/nd2_api_def.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace slideio
{
    class FileReader;

    namespace nd2
    {
        /// The header of one chunk of a chunked ND2 file: where it starts and how
        /// long its name field and data are. Taken from the chunk itself, never from
        /// the chunk map's `size` field (FD 3.3).
        struct SLIDEIO_ND2_EXPORTS ChunkHeader
        {
            uint64_t offset = 0;       // absolute offset of the magic
            uint32_t nameLength = 0;   // size of the name field in bytes
            uint64_t dataLength = 0;   // size of the data after the name field
            /// offset + 16 + nameLength. Throws on overflow.
            uint64_t dataOffset() const;
        };

        /// The container of a chunked (Ver2.x / Ver3.x) Nikon ND2 file: the signature
        /// chunk, the chunk map and a verified name -> header table. Holds no file
        /// handle; payloads are read by whoever owns the FileReader.
        ///
        /// Every map entry is verified at open (magic and name). If any entry fails,
        /// or the map cannot be read, the whole table is rebuilt by scanning the file
        /// for chunk headers. A chunk whose data runs past EOF is absent, whether it
        /// came from the map or from the scan.
        class SLIDEIO_ND2_EXPORTS ChunkFile
        {
        public:
            /// Throws RuntimeError "ND2: ..." when the file is not a chunked ND2.
            static ChunkFile read(const FileReader& file);
            int versionMajor() const { return m_versionMajor; }
            int versionMinor() const { return m_versionMinor; }
            /// The chunk with this name (including the trailing '!'); nullptr if
            /// there is none or its data runs past EOF.
            const ChunkHeader* find(const std::string& name) const;
            /// The data of a chunk found in this file.
            std::vector<uint8_t> readData(const FileReader& file, const ChunkHeader& chunk) const;
            /// `ImageDataSeq|N!` chunks by N, verified.
            const std::map<int64_t, ChunkHeader>& frames() const { return m_frames; }
            /// True if the table was rebuilt by scanning rather than taken from the map.
            bool recoveredByScan() const { return m_recoveredByScan; }
            /// Map entries that failed verification (0 if the map was not readable).
            int badMapEntries() const { return m_badMapEntries; }

        private:
            void indexFrames();

            int m_versionMajor = 0;
            int m_versionMinor = 0;
            bool m_recoveredByScan = false;
            int m_badMapEntries = 0;
            std::unordered_map<std::string, ChunkHeader> m_chunks;
            std::map<int64_t, ChunkHeader> m_frames;
        };
    }
}
