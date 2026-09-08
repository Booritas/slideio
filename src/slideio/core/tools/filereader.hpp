// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/slideio_core_def.hpp"
#include <cstdint>
#include <functional>
#include <string>

namespace slideio
{
    /**
     * A read-only file opened for positional access.
     *
     * readAt() carries its own offset and never touches a shared cursor, so a
     * single FileReader serves any number of threads with one descriptor -- no
     * pool, no per-thread handle. This is what lets the CZI and VSI read paths
     * be concurrent without replicating anything.
     *
     * Not memory-mapped, deliberately: a truncated or network-backed file would
     * raise SIGBUS or an SEH exception inside a memcpy, which is not survivable
     * in a library that reads arbitrary user files.
     */
    class SLIDEIO_CORE_EXPORTS FileReader
    {
    public:
        explicit FileReader(const std::string& path);
        ~FileReader();

        FileReader(const FileReader&) = delete;
        FileReader& operator=(const FileReader&) = delete;

        /// Fills exactly `size` bytes from `offset`, or throws. Thread-safe.
        void readAt(uint64_t offset, void* dst, size_t size) const;
        uint64_t size() const { return m_size; }
        const std::string& path() const { return m_path; }

        /// Reads `size` bytes into `dst` by calling `primitive` until satisfied.
        /// `primitive(dst, size, offset)` returns the number of bytes read, 0 at
        /// end of file (which throws), or -1 for a retryable interruption.
        ///
        /// Extracted from readAt so the retry loop can be unit-tested with a
        /// primitive that deliberately returns short. That matters because pread
        /// is permitted to return fewer bytes than requested and ifstream::read
        /// used to hide it -- this loop is the guarantee being restored, and a
        /// filesystem that produces short reads on demand is not portable to
        /// arrange in a test.
        using ReadPrimitive = std::function<int64_t(void* dst, size_t size, uint64_t offset)>;
        static void fillFrom(void* dst, size_t size, uint64_t offset,
                             const ReadPrimitive& primitive, const std::string& path);

    private:
        std::string m_path;
#if defined(WIN32)
        void* m_handle;
#else
        int m_fd;
#endif
        uint64_t m_size;
    };
}
