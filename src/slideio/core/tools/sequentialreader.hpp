// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/tools/filereader.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

namespace slideio
{
    /**
     * A cursor over a FileReader, for the single-threaded parsing that runs
     * during init(). Header parsing reads one field after another and should
     * not be contorted into positional calls; the read path uses FileReader
     * directly.
     *
     * Buffered, because a field at a time is the wrong granularity for I/O:
     * every FileReader::readAt is one pread on POSIX or one overlapped
     * ReadFile + GetOverlappedResult on Windows, and the per-record parse loops
     * of CZI and VSI read a handful of fields for each of 10^4-10^5 records.
     * Unbuffered, opening a 4.1 GB VSI issued 646,516 readAt calls and a 6.1 GB
     * CZI 133,417; with this buffer, 1,174 and 27,614 -- 1.7 s of open time
     * became 0.12 s, and 0.54 s became 0.29 s. The std::ifstream this replaced
     * amortised the same reads over its own buffer, so restoring one restores
     * what the migration to positional I/O gave away.
     *
     * kBufferSize is deliberately one page, the size of a stream buffer rather
     * than something large. A jump to an unrelated offset refills the whole
     * buffer, and CZI's directory walk alternates between the directory and
     * each sub-block's own header, so it refills roughly once per record; a
     * page is what the OS reads for a 30-byte read anyway, so that costs a
     * system call and no extra physical I/O. Doubling the buffer to 8 KB
     * doubled the bytes requested for that pattern without removing a single
     * call.
     *
     * Non-owning: the FileReader must outlive it. Single-threaded by
     * construction (one cursor per parse), so the buffer carries no
     * concurrency implication.
     */
    class SequentialReader
    {
    public:
        explicit SequentialReader(const FileReader& reader, uint64_t pos = 0)
            : m_reader(reader), m_pos(pos) {}

        template <class T>
        void read(T& value) {
            readBytes(&value, sizeof(T));
        }

        template <class T>
        T readValue() {
            T value{};
            read(value);
            return value;
        }

        void readBytes(void* dst, size_t size) {
            if (size == 0) {
                return;
            }
            if (!readBuffered(dst, size)) {
                m_reader.readAt(m_pos, dst, size);
            }
            m_pos += size;
        }

        void skip(int64_t bytes) { m_pos += bytes; }
        void setPos(uint64_t pos) { m_pos = pos; }
        uint64_t pos() const { return m_pos; }
        uint64_t size() const { return m_reader.size(); }

    private:
        static constexpr size_t kBufferSize = 4096;

        /// Serves size bytes at m_pos out of the read-ahead buffer, refilling it
        /// first if the request is not already covered. Returns false when the
        /// request cannot be served that way -- a read larger than the buffer, or
        /// one that runs past the end of the file, which is left to
        /// FileReader::readAt so that it raises its own end-of-file error.
        bool readBuffered(void* dst, size_t size) {
            if (size > kBufferSize) {
                return false;
            }
            // The bounds check comes FIRST, and is written so it cannot wrap.
            // m_pos is set from setPos() with values read straight out of the
            // file -- EtsFile::init does setPos(header.additionalHeaderPos) and
            // setPos(header.usedChunksPos), neither validated -- so it can be
            // any uint64_t at all. Testing coverage before bounds would let
            // m_pos + size wrap past the end of the address space (m_pos =
            // UINT64_MAX, size = 64 gives 63, which is inside any warm buffer),
            // report the read as already covered, skip this check entirely and
            // memcpy from m_buffer.data() + m_pos. That is the same overflow
            // FileReader::readAt was fixed for, one layer up.
            //
            // Everything below is then provably wrap-free: m_pos <= fileSize
            // and size <= fileSize - m_pos, so m_pos + size <= fileSize;
            // m_bufferPos and m_bufferedSize come from a readAt that succeeded,
            // so m_bufferPos + m_bufferedSize <= fileSize; and m_pos -
            // m_bufferPos is only evaluated once the first clause has
            // established m_pos >= m_bufferPos.
            const uint64_t fileSize = m_reader.size();
            if (m_pos > fileSize || fileSize - m_pos < size) {
                return false;
            }
            if (m_pos < m_bufferPos || m_pos + size > m_bufferPos + m_bufferedSize) {
                const size_t fill = static_cast<size_t>(
                    std::min<uint64_t>(kBufferSize, fileSize - m_pos));
                if (m_buffer.empty()) {
                    m_buffer.resize(kBufferSize);
                }
                m_reader.readAt(m_pos, m_buffer.data(), fill);
                m_bufferPos = m_pos;
                m_bufferedSize = fill;
            }
            std::memcpy(dst, m_buffer.data() + (m_pos - m_bufferPos), size);
            return true;
        }

        const FileReader& m_reader;
        uint64_t m_pos;
        std::vector<uint8_t> m_buffer;
        uint64_t m_bufferPos = 0;
        size_t m_bufferedSize = 0;
    };
}
