// src/slideio/imagetools/filestream.hpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/imagetools/slideio_imagetools_def.hpp"
#include "slideio/base/randomaccessstream.hpp"

#include <cstdint>
#include <string>

namespace slideio
{
    class SLIDEIO_IMAGETOOLS_EXPORTS FileStream : public RandomAccessStream
    {
    public:
        explicit FileStream(const std::string& path);
        ~FileStream() override;

        FileStream(const FileStream&) = delete;
        FileStream& operator=(const FileStream&) = delete;

        uint64_t size() const override;
        size_t read(uint64_t offset, size_t count, void* buf) override;
        std::string uri() const override;

    private:
#ifdef _WIN32
        void* m_handle;          // HANDLE
#else
        int m_fd;
#endif
        uint64_t m_size;
        std::string m_path;
    };
}
