// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/exceptions.hpp"
#include "slideio/core/tools/filereader.hpp"
#include "slideio/core/tools/sequentialreader.hpp"
#include "slideio/drivers/vsi/vsi_api_def.hpp"
#include <cstdint>
#include <memory>
#include <string>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio {
    namespace vsi
    {
        // The parsing API used by VSIFile and EtsFile at open time. It stays around
        // unchanged in name and shape, but its cursor is now local rather than a
        // shared stream position: it reads through a slideio::FileReader (positional,
        // thread-safe) via a private uint64_t m_pos, so a VSIStream built over a
        // FileReader shared with other code carries no state those other readers
        // could race with.
        class SLIDEIO_VSI_EXPORTS VSIStream
        {
        public:
            // Opens filePath itself. Used where nothing else needs to share the
            // resulting FileReader (VSIFile::readVolumeInfo).
            explicit VSIStream(const std::string& filePath);
            // Wraps a FileReader someone else already owns, starting at position 0.
            // Used where a positional reader (EtsFile::m_reader) must be shared
            // between this parsing cursor and the concurrent tile-read path.
            explicit VSIStream(std::shared_ptr<const FileReader> reader);

            template <typename T>
            void read(T& value) {
                m_cursor.read(value);
            }
            template <typename T>
            T readValue() {
                T value{};
                read(value);
                return value;
            }
            std::string readString(size_t dataSize);
            int64_t getPos() const;
            void setPos(int64_t pos);
            int64_t getSize();
            void skipBytes(uint32_t bytes);
            void readBytes(uint8_t* bytes, uint32_t size);
        private:
            // m_reader keeps the FileReader alive; m_cursor holds the position and
            // the read-ahead buffer. The buffer is what keeps open time sane:
            // one readAt per field is one system call per field, and EtsFile::init
            // reads numDimensions + 2 fields for each of 10^5+ used chunks (and
            // VSIFile's metadata walk reads far more), so an unbuffered cursor
            // turns an open into ~10^6 system calls. See SequentialReader.
            std::shared_ptr<const FileReader> m_reader;
            SequentialReader m_cursor;
        };
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
