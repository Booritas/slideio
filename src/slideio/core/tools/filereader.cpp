// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/tools/filereader.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/tools/tools.hpp"

#include <algorithm>
#include <cerrno>

#if defined(WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

using namespace slideio;

// Platform-independent, and the only place the "fill or throw" contract lives.
void FileReader::fillFrom(void* dst, size_t size, uint64_t offset,
                          const ReadPrimitive& primitive, const std::string& path) {
    uint8_t* cursor = static_cast<uint8_t*>(dst);
    size_t remaining = size;
    uint64_t position = offset;
    while (remaining > 0) {
        const int64_t read = primitive(cursor, remaining, position);
        if (read < 0) {
            continue;                      // retryable interruption
        }
        if (read == 0) {
            RAISE_RUNTIME_ERROR << "FileReader: unexpected end of file " << path
                                << " at " << position;
        }
        cursor += static_cast<size_t>(read);
        remaining -= static_cast<size_t>(read);
        position += static_cast<uint64_t>(read);
    }
}

#if defined(WIN32)

FileReader::FileReader(const std::string& path) : m_path(path), m_handle(nullptr), m_size(0) {
    const std::wstring wsPath = Tools::toWstring(path);
    // FILE_FLAG_OVERLAPPED is not optional. Passing an OVERLAPPED offset to a
    // handle opened without it does read from the offset, but it also moves the
    // shared file pointer, and concurrent operations on such a handle are not
    // supported -- the build would be silently racy.
    // FILE_SHARE_DELETE lets a caller (or a test) remove the file while this
    // handle is still open, same as unlink() does on POSIX -- without it,
    // std::filesystem::remove() on a still-open FileReader fails with
    // ERROR_SHARING_VIOLATION where POSIX would just succeed.
    HANDLE handle = ::CreateFileW(wsPath.c_str(), GENERIC_READ,
                                  FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                                  OPEN_EXISTING,
                                  FILE_FLAG_OVERLAPPED | FILE_FLAG_RANDOM_ACCESS,
                                  nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        RAISE_RUNTIME_ERROR << "FileReader: cannot open file " << path
                            << ". Error code: " << ::GetLastError();
    }
    LARGE_INTEGER fileSize = {};
    if (!::GetFileSizeEx(handle, &fileSize)) {
        const DWORD error = ::GetLastError();
        ::CloseHandle(handle);
        RAISE_RUNTIME_ERROR << "FileReader: cannot query size of " << path
                            << ". Error code: " << error;
    }
    m_handle = handle;
    m_size = static_cast<uint64_t>(fileSize.QuadPart);
}

FileReader::~FileReader() {
    if (m_handle) {
        ::CloseHandle(static_cast<HANDLE>(m_handle));
    }
}

void FileReader::readAt(uint64_t offset, void* dst, size_t size) const {
    if (size == 0) {
        return;
    }
    if (offset + size > m_size) {
        RAISE_RUNTIME_ERROR << "FileReader: read of " << size << " bytes at "
                            << offset << " is past the end of " << m_path
                            << " (" << m_size << " bytes)";
    }
    // One manual-reset event per thread, reused across calls. It holds no file
    // state, so it is the one thread_local this design permits.
    thread_local HANDLE event = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!event) {
        RAISE_RUNTIME_ERROR << "FileReader: cannot create an event object";
    }
    HANDLE handle = static_cast<HANDLE>(m_handle);
    const std::string& path = m_path;
    fillFrom(dst, size, offset,
             [handle, &path](void* chunkDst, size_t chunkSize,
                             uint64_t position) -> int64_t {
                 // `event` is thread_local, not a captured local -- it needs no
                 // entry in the capture list, and MSVC rejects one (a
                 // thread_local has thread storage duration, not automatic).
                 const DWORD chunk = static_cast<DWORD>(
                     std::min<size_t>(chunkSize, 0x40000000u));
                 OVERLAPPED overlapped = {};
                 overlapped.Offset = static_cast<DWORD>(position & 0xFFFFFFFFu);
                 overlapped.OffsetHigh = static_cast<DWORD>(position >> 32);
                 overlapped.hEvent = event;
                 ::ResetEvent(event);
                 DWORD read = 0;
                 if (!::ReadFile(handle, chunkDst, chunk, &read, &overlapped)) {
                     const DWORD error = ::GetLastError();
                     if (error != ERROR_IO_PENDING) {
                         RAISE_RUNTIME_ERROR << "FileReader: read failed on " << path
                                             << " at " << position
                                             << ". Error code: " << error;
                     }
                     if (!::GetOverlappedResult(handle, &overlapped, &read, TRUE)) {
                         RAISE_RUNTIME_ERROR << "FileReader: read failed on " << path
                                             << " at " << position
                                             << ". Error code: " << ::GetLastError();
                     }
                 }
                 return static_cast<int64_t>(read);
             },
             m_path);
}

#else

FileReader::FileReader(const std::string& path) : m_path(path), m_fd(-1), m_size(0) {
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        RAISE_RUNTIME_ERROR << "FileReader: cannot open file " << path;
    }
    struct stat info = {};
    if (::fstat(fd, &info) != 0) {
        ::close(fd);
        RAISE_RUNTIME_ERROR << "FileReader: cannot query size of " << path;
    }
#if defined(POSIX_FADV_RANDOM)
    ::posix_fadvise(fd, 0, 0, POSIX_FADV_RANDOM);
#endif
    m_fd = fd;
    m_size = static_cast<uint64_t>(info.st_size);
}

FileReader::~FileReader() {
    if (m_fd >= 0) {
        ::close(m_fd);
    }
}

void FileReader::readAt(uint64_t offset, void* dst, size_t size) const {
    if (size == 0) {
        return;
    }
    if (offset + size > m_size) {
        RAISE_RUNTIME_ERROR << "FileReader: read of " << size << " bytes at "
                            << offset << " is past the end of " << m_path
                            << " (" << m_size << " bytes)";
    }
    // pread does not touch the file offset, so one fd is safe across threads --
    // but it may return short, which fillFrom is what handles.
    const int fd = m_fd;
    const std::string& path = m_path;
    fillFrom(dst, size, offset,
             [fd, &path](void* chunkDst, size_t chunkSize,
                         uint64_t position) -> int64_t {
                 const ssize_t read = ::pread(fd, chunkDst, chunkSize,
                                              static_cast<off_t>(position));
                 if (read < 0) {
                     if (errno == EINTR) {
                         return -1;        // fillFrom retries without advancing
                     }
                     RAISE_RUNTIME_ERROR << "FileReader: read failed on " << path
                                         << " at " << position << ". errno " << errno;
                 }
                 return static_cast<int64_t>(read);
             },
             m_path);
}

#endif
