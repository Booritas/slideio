// src/slideio/base/randomaccessstream.hpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/base/slideio_base_def.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace slideio
{
    // Abstract random-access read-only byte source.
    // Implementations must be thread-safe: concurrent read() calls on the same
    // instance are permitted.
    class SLIDEIO_BASE_EXPORTS RandomAccessStream
    {
    public:
        virtual ~RandomAccessStream();

        // Total size of the underlying object in bytes.
        virtual uint64_t size() const = 0;

        // Read up to `count` bytes starting at `offset` into `buf`.
        // Returns the number of bytes actually read. Returns 0 only when
        // `count == 0` or when `offset >= size()`.
        // Thread-safe: concurrent read() calls on the same instance are permitted.
        // Throws slideio::RuntimeError on non-EOF errors (network failure, etc.).
        virtual size_t read(uint64_t offset, size_t count, void* buf) = 0;

        // Advisory hint that bytes in [offset, offset+count) will soon be read.
        // Implementations may ignore (default) or warm an internal cache.
        virtual void prefetch(uint64_t offset, size_t count);

        // Human-readable identifier for logs and error messages.
        virtual std::string uri() const = 0;
    };
}
