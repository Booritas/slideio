// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/tools/filereader.hpp"

namespace slideio
{
    /**
     * A cursor over a FileReader, for the single-threaded parsing that runs
     * during init(). Header parsing reads one field after another and should
     * not be contorted into positional calls; the read path uses FileReader
     * directly.
     *
     * Non-owning: the FileReader must outlive it.
     */
    class SequentialReader
    {
    public:
        explicit SequentialReader(const FileReader& reader, uint64_t pos = 0)
            : m_reader(reader), m_pos(pos) {}

        template <class T>
        void read(T& value) {
            m_reader.readAt(m_pos, &value, sizeof(T));
            m_pos += sizeof(T);
        }

        template <class T>
        T readValue() {
            T value{};
            read(value);
            return value;
        }

        void readBytes(void* dst, size_t size) {
            m_reader.readAt(m_pos, dst, size);
            m_pos += size;
        }

        void skip(int64_t bytes) { m_pos += bytes; }
        void setPos(uint64_t pos) { m_pos = pos; }
        uint64_t pos() const { return m_pos; }
        uint64_t size() const { return m_reader.size(); }

    private:
        const FileReader& m_reader;
        uint64_t m_pos;
    };
}
