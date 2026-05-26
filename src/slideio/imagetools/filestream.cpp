// src/slideio/imagetools/filestream.cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/imagetools/filestream.hpp"
#include "slideio/base/exceptions.hpp"

#include <filesystem>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <fcntl.h>
  #include <sys/stat.h>
  #include <unistd.h>
  #include <cerrno>
#endif

namespace slideio
{

FileStream::FileStream(const std::string& path)
    :
#ifdef _WIN32
      m_handle(nullptr),
#else
      m_fd(-1),
#endif
      m_size(0), m_path(path)
{
    if (!std::filesystem::exists(path)) {
        RAISE_RUNTIME_ERROR << "FileStream: file does not exist: " << path;
    }
#ifdef _WIN32
    m_handle = ::CreateFileA(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
    if (m_handle == INVALID_HANDLE_VALUE) {
        RAISE_RUNTIME_ERROR << "FileStream: CreateFile failed for " << path;
    }
    LARGE_INTEGER sz; ::GetFileSizeEx(m_handle, &sz);
    m_size = static_cast<uint64_t>(sz.QuadPart);
#else
    m_fd = ::open(path.c_str(), O_RDONLY);
    if (m_fd < 0) {
        RAISE_RUNTIME_ERROR << "FileStream: open failed for " << path
                            << " errno=" << errno;
    }
    struct stat st{};
    ::fstat(m_fd, &st);
    m_size = static_cast<uint64_t>(st.st_size);
#endif
}

FileStream::~FileStream()
{
#ifdef _WIN32
    if (m_handle && m_handle != INVALID_HANDLE_VALUE) ::CloseHandle(m_handle);
#else
    if (m_fd >= 0) ::close(m_fd);
#endif
}

uint64_t FileStream::size() const { return m_size; }

std::string FileStream::uri() const { return std::string("file://") + m_path; }

size_t FileStream::read(uint64_t offset, size_t count, void* buf)
{
    if (count == 0 || offset >= m_size) return 0;
    if (offset + count > m_size) count = static_cast<size_t>(m_size - offset);

#ifdef _WIN32
    OVERLAPPED ov{};
    ov.Offset = static_cast<DWORD>(offset & 0xFFFFFFFF);
    ov.OffsetHigh = static_cast<DWORD>(offset >> 32);
    DWORD got = 0;
    BOOL ok = ::ReadFile(m_handle, buf, static_cast<DWORD>(count), &got, &ov);
    if (!ok) {
        DWORD err = ::GetLastError();
        if (err == ERROR_HANDLE_EOF) return 0;
        if (err == ERROR_IO_PENDING) {
            ::GetOverlappedResult(m_handle, &ov, &got, TRUE);
        } else {
            RAISE_RUNTIME_ERROR << "FileStream::read: ReadFile failed err=" << err;
        }
    }
    return static_cast<size_t>(got);
#else
    ssize_t n = ::pread(m_fd, buf, count, static_cast<off_t>(offset));
    if (n < 0) {
        RAISE_RUNTIME_ERROR << "FileStream::read: pread failed errno=" << errno;
    }
    return static_cast<size_t>(n);
#endif
}

} // namespace slideio
