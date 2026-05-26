# S3 / HTTPS Streaming — v1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add presigned-URL HTTPS streaming for the TIFF-family drivers (svs, scn, ndpi, pke, ometiff, afi) via a new `RandomAccessStream` abstraction, libcurl-backed `HttpStream` with a per-stream LRU block cache, and a `TIFFClientOpen`-based adapter that lets every libtiff caller in the codebase read through the abstraction.

**Architecture:** A new interface `slideio::RandomAccessStream` (stateless `read(offset, size, buf)`, `size()`, `prefetch()`, `uri()`) is added at the lowest module (`slideio-base`). Two implementations live in `slideio-imagetools`: `FileStream` (local files, `pread`/`ReadFile`) and `HttpStream` (libcurl + 1 MB-block, 256 MB LRU cache). A `TIFFClientAdapter` wraps any `RandomAccessStream` into a libtiff `TIFF*` via `TIFFClientOpen`. `ImageDriverManager::openSlide` detects `s3://` / `http(s)://` prefixes and constructs the appropriate stream before handing it to the driver via a new `ImageDriver::openFile(std::shared_ptr<RandomAccessStream>)` virtual. Drivers advertise readiness via `supportsStream() = true`.

**Tech Stack:** C++17, libtiff, libcurl (new), GoogleTest, CMake, Conan v2. Python 3 (only for the local HTTP test fixture).

**Reference spec:** `software-docs/specs/2026-05-25-s3-streaming-design.md`

**Branch:** `s3`

**Scope of this plan:** v1 only (foundation + 6 TIFF-family drivers). v2 (czi, vsi) and v3 (gdal, zvi, dcm) will be planned separately after v1 ships.

---

## File Structure

### New files (interface + implementations)

| Path | Responsibility |
|---|---|
| `src/slideio/base/randomaccessstream.hpp` | Abstract `RandomAccessStream` interface (header only) |
| `src/slideio/imagetools/filestream.hpp/.cpp` | `FileStream` — local-file implementation |
| `src/slideio/imagetools/blockcache.hpp/.cpp` | `BlockCache` — per-stream LRU keyed by block index |
| `src/slideio/imagetools/httpstream.hpp/.cpp` | `HttpStream` — libcurl-backed, owns a `BlockCache` |
| `src/slideio/imagetools/uridispatcher.hpp/.cpp` | `createStream(uri)` factory + URI prefix detection |
| `src/slideio/imagetools/tiffclientadapter.hpp/.cpp` | `openTiffFromStream(stream)` — wraps `RandomAccessStream` in `TIFFClientOpen` |

### New test files

| Path | Responsibility |
|---|---|
| `src/tests/main/memorystream.hpp` | Test-only `MemoryStream` (`RandomAccessStream` over `std::vector<uint8_t>`) |
| `src/tests/main/random_access_stream_contract.hpp` | Shared contract fixture template — instantiated against each backend |
| `src/tests/main/test_random_access_stream_contract.cpp` | Instantiations of the contract fixture (`FileStream`, `MemoryStream`) |
| `src/tests/main/test_filestream.cpp` | `FileStream`-specific tests (Windows paths, errors, etc.) |
| `src/tests/main/test_blockcache.cpp` | `BlockCache` unit tests |
| `src/tests/main/test_httpstream.cpp` | `HttpStream` tests against the local HTTP fixture |
| `src/tests/main/test_uri_dispatcher.cpp` | URI prefix detection + `createStream` factory |
| `src/tests/main/test_tiff_client_adapter.cpp` | TIFF adapter against a known TIFF in `MemoryStream` |
| `src/tests/main/test_s3_streaming_integration.cpp` | End-to-end SVS/NDPI/OME-TIFF/AFI over local HTTP |
| `src/tests/main/http_fixture/test_http_server.py` | Python `http.server` with byte-range support |

### Modified files

| Path | Why |
|---|---|
| `src/slideio/imagetools/conanfile.py` | Add `libcurl/8.10.1` dependency |
| `src/slideio/imagetools/CMakeLists.txt` | List new sources; `find_package(CURL)`; link `CURL::libcurl` |
| `src/slideio/imagetools/tifftools.hpp/.cpp` | New `openTiffFile(stream)` overload delegating to `TIFFClientAdapter` |
| `src/slideio/drivers/ndpi/ndpitifftools.hpp/.cpp` | Same overload pattern for NDPI's parallel TIFF wrapper |
| `src/slideio/core/imagedriver.hpp/.cpp` | Add `supportsStream()`, `openFile(stream)` virtuals; default `openFile(path)` wraps in `FileStream` |
| `src/slideio/core/tools/tools.hpp/.cpp` | `matchPattern` strips query string for URIs |
| `src/slideio/slideio/imagedrivermanager.hpp/.cpp` | URI dispatch; `setHttpCacheEnabled` |
| `src/slideio/drivers/svs/svsimagedriver.hpp/.cpp` | `supportsStream()=true`; `openFile(stream)` override |
| `src/slideio/drivers/svs/svsslide.hpp/.cpp` | `openFile(stream, id)` factory |
| `src/slideio/drivers/scn/scnimagedriver.hpp/.cpp`, `scnslide.hpp/.cpp` | Same pattern |
| `src/slideio/drivers/ndpi/ndpiimagedriver.hpp/.cpp`, `ndpifile.hpp/.cpp` | Same pattern (uses `NDPITiffTools`) |
| `src/slideio/drivers/pke/pkeimagedriver.hpp/.cpp`, `pkeslide.hpp/.cpp` | Same pattern |
| `src/slideio/drivers/ome-tiff/otimagedriver.hpp/.cpp`, `otslide.hpp/.cpp` | Same pattern + companion XML via stream |
| `src/slideio/drivers/afi/afiimagedriver.hpp/.cpp`, `afislide.hpp/.cpp` | Same pattern + URI-prefix-aware reference resolution |
| `src/tests/main/CMakeLists.txt` | List new test sources; link `CURL::libcurl` if needed for test harness |

---

## Coding conventions (apply to every task)

- All new classes live in namespace `slideio` and use the `m_` member prefix.
- Methods are camelCase starting lowercase (e.g., `openFile`, `setHttpCacheEnabled`).
- Errors raised with the existing `RAISE_RUNTIME_ERROR` macro (from `slideio/base/exceptions.hpp`).
- Info-level logging via `SLIDEIO_LOG(INFO)` (from `slideio/base/log.hpp`).
- Public class symbols in `slideio-imagetools` use `SLIDEIO_IMAGETOOLS_EXPORTS`; in `slideio-base` use whatever the base module's export macro is (check `slideio_base_def.hpp` before adding).
- Each new header begins with the project license comment block (copy from any existing header) and uses `#pragma once` or the existing `OPENCV_slideio_*_HPP` guard convention — match the file's neighbor.
- No emojis or decorative comments. Comments only for non-obvious *why*.

## Build & test commands (used throughout)

```bash
# Configure + build (full):
python3 install.py -a install -c release

# Incremental build only (after configure):
python3 install.py -a build-only -c release

# Run all main tests:
./build/release/bin/slideio_tests

# Run a filtered subset:
./build/release/bin/slideio_tests --gtest_filter="RandomAccessStreamContract*"

# After a Conan change:
python3 install.py -a conan && python3 install.py -a configure-only -c release
```

On Windows, replace forward slashes with backslashes for the test executable path and use `python` instead of `python3`.

## Commit cadence

One commit per task. Commit message style follows the recent history (subject line under ~70 chars, optional body for the *why*). All commits go on branch `s3`.

---

## Phase A — Foundation interface

### Task A1: Add the `RandomAccessStream` interface

**Files:**
- Create: `src/slideio/base/randomaccessstream.hpp`
- Test: (deferred — first concrete instantiation in Task A2)

- [ ] **Step 1: Create the interface header.**

```cpp
// src/slideio/base/randomaccessstream.hpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace slideio
{
    // Abstract random-access read-only byte source.
    // Implementations must be thread-safe: concurrent read() calls on the same
    // instance are permitted.
    class RandomAccessStream
    {
    public:
        virtual ~RandomAccessStream() = default;

        // Total size of the underlying object in bytes.
        virtual uint64_t size() const = 0;

        // Read up to `count` bytes starting at `offset` into `buf`.
        // Returns the number of bytes actually read; 0 only at or past EOF.
        // Throws on non-EOF errors (network failure, auth failure, etc.).
        virtual size_t read(uint64_t offset, size_t count, void* buf) = 0;

        // Advisory hint that bytes in [offset, offset+count) will soon be read.
        // Implementations may ignore (default) or warm an internal cache.
        virtual void prefetch(uint64_t /*offset*/, size_t /*count*/) {}

        // Human-readable identifier for logs and error messages.
        virtual std::string uri() const = 0;
    };
}
```

- [ ] **Step 2: Verify it compiles standalone.**

The header has no implementation, so the build doesn't need to change yet. Confirm by including it in `src/slideio/base/base.hpp`:

Modify `src/slideio/base/base.hpp` to add:

```cpp
#include "slideio/base/randomaccessstream.hpp"
```

Run `python3 install.py -a build-only -c release` and confirm a clean build.

- [ ] **Step 3: Commit.**

```bash
git add src/slideio/base/randomaccessstream.hpp src/slideio/base/base.hpp
git commit -m "Add RandomAccessStream interface in slideio-base"
```

### Task A2: Add `MemoryStream` test helper

**Files:**
- Create: `src/tests/main/memorystream.hpp`

- [ ] **Step 1: Create the test helper.**

```cpp
// src/tests/main/memorystream.hpp
#pragma once

#include "slideio/base/randomaccessstream.hpp"
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace slideio { namespace tests {

class MemoryStream : public slideio::RandomAccessStream
{
public:
    MemoryStream(std::vector<uint8_t> data, std::string uri = "memory://")
        : m_data(std::move(data)), m_uri(std::move(uri)) {}

    uint64_t size() const override { return m_data.size(); }

    size_t read(uint64_t offset, size_t count, void* buf) override
    {
        if (offset >= m_data.size()) return 0;
        const size_t avail = m_data.size() - static_cast<size_t>(offset);
        const size_t toCopy = (count < avail) ? count : avail;
        std::memcpy(buf, m_data.data() + offset, toCopy);
        return toCopy;
    }

    std::string uri() const override { return m_uri; }

private:
    std::vector<uint8_t> m_data;
    std::string m_uri;
};

}} // namespace slideio::tests
```

- [ ] **Step 2: No build wiring needed yet** — the header has no `.cpp` and is included only by future tests.

- [ ] **Step 3: Commit.**

```bash
git add src/tests/main/memorystream.hpp
git commit -m "Add MemoryStream test helper for RandomAccessStream"
```

### Task A3: Shared contract-test fixture

**Files:**
- Create: `src/tests/main/random_access_stream_contract.hpp`
- Create: `src/tests/main/test_random_access_stream_contract.cpp`
- Modify: `src/tests/main/CMakeLists.txt`

- [ ] **Step 1: Write the type-parameterized contract fixture.**

```cpp
// src/tests/main/random_access_stream_contract.hpp
#pragma once

#include <gtest/gtest.h>
#include "slideio/base/randomaccessstream.hpp"

#include <atomic>
#include <memory>
#include <random>
#include <thread>
#include <vector>

namespace slideio { namespace tests {

// Each backend provides a Factory subclass that returns a stream backed by
// the given bytes. Factories live next to each backend's test file.
struct StreamFactory {
    virtual ~StreamFactory() = default;
    virtual std::shared_ptr<slideio::RandomAccessStream> make(
        const std::vector<uint8_t>& bytes) = 0;
};

template <typename FactoryT>
class RandomAccessStreamContract : public ::testing::Test
{
protected:
    FactoryT factory;
    std::vector<uint8_t> mkBytes(size_t n)
    {
        std::vector<uint8_t> v(n);
        for (size_t i = 0; i < n; ++i) v[i] = static_cast<uint8_t>(i * 31 + 7);
        return v;
    }
};

TYPED_TEST_SUITE_P(RandomAccessStreamContract);

TYPED_TEST_P(RandomAccessStreamContract, ReadAtOffsetZero) {
    auto bytes = this->mkBytes(4096);
    auto s = this->factory.make(bytes);
    std::vector<uint8_t> buf(16);
    EXPECT_EQ(s->read(0, buf.size(), buf.data()), buf.size());
    EXPECT_EQ(std::memcmp(buf.data(), bytes.data(), buf.size()), 0);
}

TYPED_TEST_P(RandomAccessStreamContract, ReadPastEofReturnsZero) {
    auto bytes = this->mkBytes(100);
    auto s = this->factory.make(bytes);
    std::vector<uint8_t> buf(16);
    EXPECT_EQ(s->read(200, buf.size(), buf.data()), 0u);
}

TYPED_TEST_P(RandomAccessStreamContract, ReadPartialAtEof) {
    auto bytes = this->mkBytes(100);
    auto s = this->factory.make(bytes);
    std::vector<uint8_t> buf(50);
    EXPECT_EQ(s->read(80, buf.size(), buf.data()), 20u);
    EXPECT_EQ(std::memcmp(buf.data(), bytes.data() + 80, 20), 0);
}

TYPED_TEST_P(RandomAccessStreamContract, ReadZeroBytes) {
    auto bytes = this->mkBytes(100);
    auto s = this->factory.make(bytes);
    EXPECT_EQ(s->read(0, 0, nullptr), 0u);
}

TYPED_TEST_P(RandomAccessStreamContract, RandomOffsetsReproducible) {
    auto bytes = this->mkBytes(64 * 1024);
    auto s = this->factory.make(bytes);
    std::mt19937 rng(12345);
    std::uniform_int_distribution<uint64_t> offDist(0, bytes.size() - 1);
    std::uniform_int_distribution<size_t> sizeDist(1, 1024);
    for (int i = 0; i < 100; ++i) {
        uint64_t off = offDist(rng);
        size_t n = sizeDist(rng);
        if (off + n > bytes.size()) n = bytes.size() - off;
        std::vector<uint8_t> buf(n);
        ASSERT_EQ(s->read(off, n, buf.data()), n);
        ASSERT_EQ(std::memcmp(buf.data(), bytes.data() + off, n), 0);
    }
}

TYPED_TEST_P(RandomAccessStreamContract, ConcurrentReads) {
    auto bytes = this->mkBytes(64 * 1024);
    auto s = this->factory.make(bytes);
    std::atomic<bool> ok{true};
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&, t]() {
            std::mt19937 rng(t * 7 + 1);
            std::uniform_int_distribution<uint64_t> offDist(0, bytes.size() - 1);
            std::uniform_int_distribution<size_t> sizeDist(1, 512);
            for (int i = 0; i < 200 && ok; ++i) {
                uint64_t off = offDist(rng);
                size_t n = sizeDist(rng);
                if (off + n > bytes.size()) n = bytes.size() - off;
                std::vector<uint8_t> buf(n);
                if (s->read(off, n, buf.data()) != n) { ok = false; return; }
                if (std::memcmp(buf.data(), bytes.data() + off, n) != 0) ok = false;
            }
        });
    }
    for (auto& th : threads) th.join();
    EXPECT_TRUE(ok);
}

TYPED_TEST_P(RandomAccessStreamContract, PrefetchIsCallable) {
    auto bytes = this->mkBytes(100);
    auto s = this->factory.make(bytes);
    EXPECT_NO_THROW(s->prefetch(0, 50));
}

REGISTER_TYPED_TEST_SUITE_P(RandomAccessStreamContract,
    ReadAtOffsetZero, ReadPastEofReturnsZero, ReadPartialAtEof,
    ReadZeroBytes, RandomOffsetsReproducible, ConcurrentReads,
    PrefetchIsCallable);

}} // namespace
```

- [ ] **Step 2: Instantiate the fixture for `MemoryStream`.**

```cpp
// src/tests/main/test_random_access_stream_contract.cpp
#include "memorystream.hpp"
#include "random_access_stream_contract.hpp"

namespace slideio { namespace tests {

struct MemoryStreamFactory : StreamFactory {
    std::shared_ptr<slideio::RandomAccessStream> make(
        const std::vector<uint8_t>& bytes) override
    {
        return std::make_shared<MemoryStream>(bytes);
    }
};

INSTANTIATE_TYPED_TEST_SUITE_P(MemoryStream, RandomAccessStreamContract,
                               ::testing::Types<MemoryStreamFactory>);

}} // namespace
```

- [ ] **Step 3: Add to test CMake.**

In `src/tests/main/CMakeLists.txt`, append to `TEST_SOURCES`:

```
  test_random_access_stream_contract.cpp
```

- [ ] **Step 4: Build and run.**

```bash
python3 install.py -a build-only -c release
./build/release/bin/slideio_tests --gtest_filter="MemoryStream/RandomAccessStreamContract*"
```

Expected: all 7 contract tests pass for MemoryStream.

- [ ] **Step 5: Commit.**

```bash
git add src/tests/main/memorystream.hpp \
        src/tests/main/random_access_stream_contract.hpp \
        src/tests/main/test_random_access_stream_contract.cpp \
        src/tests/main/CMakeLists.txt
git commit -m "Add RandomAccessStream contract-test fixture"
```

---

## Phase B — `FileStream`

### Task B1: `FileStream` — failing tests first

**Files:**
- Create: `src/tests/main/test_filestream.cpp`
- Modify: `src/tests/main/CMakeLists.txt`
- Modify: `src/tests/main/test_random_access_stream_contract.cpp` (add factory)

- [ ] **Step 1: Write the FileStream factory + instantiation.**

In `src/tests/main/test_random_access_stream_contract.cpp`, add:

```cpp
#include "slideio/imagetools/filestream.hpp"
#include <filesystem>
#include <fstream>

namespace slideio { namespace tests {

struct FileStreamFactory : StreamFactory {
    std::vector<std::filesystem::path> tempFiles;

    ~FileStreamFactory() override {
        for (const auto& p : tempFiles) std::error_code ec; std::filesystem::remove(p, ec);
    }

    std::shared_ptr<slideio::RandomAccessStream> make(
        const std::vector<uint8_t>& bytes) override
    {
        auto path = std::filesystem::temp_directory_path() /
                    ("slideio_fs_" + std::to_string(reinterpret_cast<uintptr_t>(this))
                     + "_" + std::to_string(tempFiles.size()) + ".bin");
        {
            std::ofstream out(path, std::ios::binary);
            out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        }
        tempFiles.push_back(path);
        return std::make_shared<slideio::FileStream>(path.string());
    }
};

INSTANTIATE_TYPED_TEST_SUITE_P(FileStream, RandomAccessStreamContract,
                               ::testing::Types<FileStreamFactory>);

}} // namespace
```

- [ ] **Step 2: Write FileStream-specific tests.**

```cpp
// src/tests/main/test_filestream.cpp
#include <gtest/gtest.h>
#include "slideio/imagetools/filestream.hpp"
#include <filesystem>
#include <fstream>

TEST(FileStreamTest, ThrowsOnNonexistent) {
    EXPECT_ANY_THROW(slideio::FileStream("Z:/definitely/does/not/exist.bin"));
}

TEST(FileStreamTest, ReportsCorrectSize) {
    auto path = std::filesystem::temp_directory_path() / "slideio_fs_size.bin";
    { std::ofstream out(path, std::ios::binary); out << "hello world"; }
    slideio::FileStream s(path.string());
    EXPECT_EQ(s.size(), 11u);
    std::filesystem::remove(path);
}

TEST(FileStreamTest, UriHasFileScheme) {
    auto path = std::filesystem::temp_directory_path() / "slideio_fs_uri.bin";
    { std::ofstream out(path, std::ios::binary); out << "x"; }
    slideio::FileStream s(path.string());
    EXPECT_NE(s.uri().find("file://"), std::string::npos);
    std::filesystem::remove(path);
}
```

- [ ] **Step 3: Add to test CMake** (append `test_filestream.cpp` to `TEST_SOURCES` in `src/tests/main/CMakeLists.txt`).

- [ ] **Step 4: Build — should FAIL (FileStream not yet implemented).**

```bash
python3 install.py -a build-only -c release
```

Expected: link/compile error referencing `slideio::FileStream`. Good — confirms the tests are the gating contract.

### Task B2: Implement `FileStream`

**Files:**
- Create: `src/slideio/imagetools/filestream.hpp`
- Create: `src/slideio/imagetools/filestream.cpp`
- Modify: `src/slideio/imagetools/CMakeLists.txt`

- [ ] **Step 1: Header.**

```cpp
// src/slideio/imagetools/filestream.hpp
// (license header)
#pragma once

#include "slideio/imagetools/slideio_imagetools_def.hpp"
#include "slideio/base/randomaccessstream.hpp"

#include <cstdint>
#include <mutex>
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
        // Mutex protects only the *cursor* in fallback paths if needed; pread/ReadFile
        // with explicit OVERLAPPED do not need it.
    };
}
```

- [ ] **Step 2: Implementation (POSIX `pread`, Windows `ReadFile` with `OVERLAPPED`).**

```cpp
// src/slideio/imagetools/filestream.cpp
// (license header)
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
    : m_size(0), m_path(path)
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
```

- [ ] **Step 3: CMake.**

In `src/slideio/imagetools/CMakeLists.txt`, append to `SOURCE_FILES`:

```
   ${CMAKE_CURRENT_SOURCE_DIR}/filestream.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/filestream.cpp
```

- [ ] **Step 4: Build and run.**

```bash
python3 install.py -a build-only -c release
./build/release/bin/slideio_tests --gtest_filter="FileStream*"
```

Expected: all contract tests + FileStream-specific tests pass.

- [ ] **Step 5: Commit.**

```bash
git add src/slideio/imagetools/filestream.{hpp,cpp} \
        src/slideio/imagetools/CMakeLists.txt \
        src/tests/main/test_filestream.cpp \
        src/tests/main/test_random_access_stream_contract.cpp \
        src/tests/main/CMakeLists.txt
git commit -m "Add FileStream (local-file RandomAccessStream backend)"
```

---

## Phase C — `BlockCache`

### Task C1: BlockCache — failing tests

**Files:**
- Create: `src/tests/main/test_blockcache.cpp`
- Modify: `src/tests/main/CMakeLists.txt`

- [ ] **Step 1: Write the tests.**

```cpp
// src/tests/main/test_blockcache.cpp
#include <gtest/gtest.h>
#include "slideio/imagetools/blockcache.hpp"

#include <vector>

using slideio::BlockCache;

static std::vector<uint8_t> makeBlock(uint8_t fill, size_t size = 1024) {
    return std::vector<uint8_t>(size, fill);
}

TEST(BlockCacheTest, MissThenHit) {
    BlockCache cache(/*capacityBlocks*/ 4);
    EXPECT_FALSE(cache.contains(0));
    cache.insert(0, makeBlock(0xAA));
    EXPECT_TRUE(cache.contains(0));
    std::vector<uint8_t> got;
    EXPECT_TRUE(cache.get(0, got));
    EXPECT_EQ(got.size(), 1024u);
    EXPECT_EQ(got[0], 0xAA);
}

TEST(BlockCacheTest, LruEvictsOldest) {
    BlockCache cache(/*capacityBlocks*/ 2);
    cache.insert(0, makeBlock(0));
    cache.insert(1, makeBlock(1));
    cache.insert(2, makeBlock(2)); // should evict block 0
    EXPECT_FALSE(cache.contains(0));
    EXPECT_TRUE(cache.contains(1));
    EXPECT_TRUE(cache.contains(2));
}

TEST(BlockCacheTest, GetUpdatesRecency) {
    BlockCache cache(/*capacityBlocks*/ 2);
    cache.insert(0, makeBlock(0));
    cache.insert(1, makeBlock(1));
    std::vector<uint8_t> buf;
    cache.get(0, buf);          // touch block 0
    cache.insert(2, makeBlock(2));  // should evict block 1 (oldest now)
    EXPECT_TRUE(cache.contains(0));
    EXPECT_FALSE(cache.contains(1));
    EXPECT_TRUE(cache.contains(2));
}

TEST(BlockCacheTest, Clear) {
    BlockCache cache(4);
    cache.insert(0, makeBlock(0));
    cache.insert(1, makeBlock(1));
    cache.clear();
    EXPECT_FALSE(cache.contains(0));
    EXPECT_FALSE(cache.contains(1));
}
```

- [ ] **Step 2: Add to CMake** (`test_blockcache.cpp` to `TEST_SOURCES`).

- [ ] **Step 3: Build — should fail (`BlockCache` not defined).**

### Task C2: Implement `BlockCache`

**Files:**
- Create: `src/slideio/imagetools/blockcache.hpp`
- Create: `src/slideio/imagetools/blockcache.cpp`
- Modify: `src/slideio/imagetools/CMakeLists.txt`

- [ ] **Step 1: Header.**

```cpp
// src/slideio/imagetools/blockcache.hpp
// (license header)
#pragma once

#include "slideio/imagetools/slideio_imagetools_def.hpp"
#include <cstddef>
#include <cstdint>
#include <list>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace slideio
{
    // Per-stream LRU cache keyed by block index. Thread-safe.
    class SLIDEIO_IMAGETOOLS_EXPORTS BlockCache
    {
    public:
        explicit BlockCache(size_t capacityBlocks);

        bool contains(uint64_t blockIndex) const;
        // Copies block bytes into `out`. Returns true if present (and updates LRU).
        bool get(uint64_t blockIndex, std::vector<uint8_t>& out);
        // Inserts (or replaces). May evict the least-recently-used block.
        void insert(uint64_t blockIndex, std::vector<uint8_t> bytes);
        void clear();
        size_t size() const;            // current block count (for tests)
        size_t capacity() const { return m_capacityBlocks; }

    private:
        using ListIt = std::list<uint64_t>::iterator;
        size_t m_capacityBlocks;
        std::list<uint64_t> m_lruOrder; // front = most recent
        std::unordered_map<uint64_t, std::pair<std::vector<uint8_t>, ListIt>> m_map;
        mutable std::mutex m_mutex;
    };
}
```

- [ ] **Step 2: Implementation.**

```cpp
// src/slideio/imagetools/blockcache.cpp
// (license header)
#include "slideio/imagetools/blockcache.hpp"

namespace slideio
{

BlockCache::BlockCache(size_t capacityBlocks) : m_capacityBlocks(capacityBlocks) {}

bool BlockCache::contains(uint64_t blockIndex) const
{
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_map.find(blockIndex) != m_map.end();
}

bool BlockCache::get(uint64_t blockIndex, std::vector<uint8_t>& out)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_map.find(blockIndex);
    if (it == m_map.end()) return false;
    m_lruOrder.erase(it->second.second);
    m_lruOrder.push_front(blockIndex);
    it->second.second = m_lruOrder.begin();
    out = it->second.first;
    return true;
}

void BlockCache::insert(uint64_t blockIndex, std::vector<uint8_t> bytes)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_map.find(blockIndex);
    if (it != m_map.end()) {
        m_lruOrder.erase(it->second.second);
        m_lruOrder.push_front(blockIndex);
        it->second.first = std::move(bytes);
        it->second.second = m_lruOrder.begin();
        return;
    }
    if (m_map.size() >= m_capacityBlocks && !m_lruOrder.empty()) {
        uint64_t victim = m_lruOrder.back();
        m_lruOrder.pop_back();
        m_map.erase(victim);
    }
    m_lruOrder.push_front(blockIndex);
    m_map.emplace(blockIndex, std::make_pair(std::move(bytes), m_lruOrder.begin()));
}

void BlockCache::clear()
{
    std::lock_guard<std::mutex> lk(m_mutex);
    m_lruOrder.clear();
    m_map.clear();
}

size_t BlockCache::size() const
{
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_map.size();
}

} // namespace slideio
```

- [ ] **Step 3: CMake.**

Append to `SOURCE_FILES` in `src/slideio/imagetools/CMakeLists.txt`:

```
   ${CMAKE_CURRENT_SOURCE_DIR}/blockcache.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/blockcache.cpp
```

- [ ] **Step 4: Build and test.**

```bash
python3 install.py -a build-only -c release
./build/release/bin/slideio_tests --gtest_filter="BlockCacheTest*"
```

Expected: all 4 tests pass.

- [ ] **Step 5: Commit.**

```bash
git add src/slideio/imagetools/blockcache.{hpp,cpp} \
        src/slideio/imagetools/CMakeLists.txt \
        src/tests/main/test_blockcache.cpp \
        src/tests/main/CMakeLists.txt
git commit -m "Add BlockCache (LRU cache keyed by block index)"
```

---

## Phase D — HTTP test fixture

### Task D1: Local HTTP server fixture (Python)

**Files:**
- Create: `src/tests/main/http_fixture/test_http_server.py`

- [ ] **Step 1: Write the fixture script.**

```python
#!/usr/bin/env python3
# src/tests/main/http_fixture/test_http_server.py
# Local HTTP server with byte-range and fault-injection support for HttpStream tests.
import argparse
import os
import re
import sys
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer

RANGE_RE = re.compile(r"bytes=(\d*)-(\d*)")

class Handler(BaseHTTPRequestHandler):
    server_version = "SlideIOTestHTTP/1.0"
    fail_count = 0          # how many initial requests to fail with 503
    served = 0

    def log_message(self, fmt, *args):
        pass  # quiet

    def _resolve(self):
        # URL path -> filesystem path under root_dir
        rel = self.path.split("?", 1)[0].lstrip("/")
        return os.path.normpath(os.path.join(self.server.root_dir, rel))

    def do_HEAD(self):
        full = self._resolve()
        if not os.path.isfile(full):
            self.send_response(404); self.end_headers(); return
        size = os.path.getsize(full)
        self.send_response(200)
        self.send_header("Content-Length", str(size))
        self.send_header("Accept-Ranges", "bytes")
        self.end_headers()

    def do_GET(self):
        if Handler.fail_count > 0:
            Handler.fail_count -= 1
            self.send_response(503); self.end_headers(); return
        full = self._resolve()
        if not os.path.isfile(full):
            self.send_response(404); self.end_headers(); return
        size = os.path.getsize(full)
        rng = self.headers.get("Range")
        if rng:
            m = RANGE_RE.match(rng)
            if not m:
                self.send_response(416); self.end_headers(); return
            start = int(m.group(1)) if m.group(1) else 0
            end = int(m.group(2)) if m.group(2) else size - 1
            if start > end or end >= size:
                self.send_response(416); self.end_headers(); return
            length = end - start + 1
            self.send_response(206)
            self.send_header("Content-Range", f"bytes {start}-{end}/{size}")
            self.send_header("Content-Length", str(length))
            self.send_header("Accept-Ranges", "bytes")
            self.end_headers()
            with open(full, "rb") as f:
                f.seek(start); self.wfile.write(f.read(length))
        else:
            self.send_response(200)
            self.send_header("Content-Length", str(size))
            self.send_header("Accept-Ranges", "bytes")
            self.end_headers()
            with open(full, "rb") as f:
                self.wfile.write(f.read())
        Handler.served += 1

    def do_POST(self):
        # Control endpoint: /__control__/fail-next/N sets fail_count = N
        if self.path.startswith("/__control__/fail-next/"):
            try:
                Handler.fail_count = int(self.path.rsplit("/", 1)[-1])
                self.send_response(204); self.end_headers(); return
            except ValueError:
                pass
        self.send_response(404); self.end_headers()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--port", type=int, default=0)
    args = ap.parse_args()
    server = HTTPServer(("127.0.0.1", args.port), Handler)
    server.root_dir = args.root
    # Print the actual port on a single line so the test process can read it.
    print(f"PORT={server.server_address[1]}", flush=True)
    server.serve_forever()

if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Smoke-test the fixture manually.**

```bash
mkdir -p /tmp/slideio-http-test
echo "hello" > /tmp/slideio-http-test/hello.txt
python3 src/tests/main/http_fixture/test_http_server.py --root /tmp/slideio-http-test --port 9999 &
sleep 1
curl -s -H "Range: bytes=0-3" http://127.0.0.1:9999/hello.txt
# Expected output: "hell" (first 4 bytes)
kill %1
```

- [ ] **Step 3: Commit.**

```bash
git add src/tests/main/http_fixture/test_http_server.py
git commit -m "Add Python HTTP test fixture with byte-range and 503 injection"
```

### Task D2: C++ helper to launch the fixture from tests

**Files:**
- Create: `src/tests/main/http_fixture/http_fixture.hpp`
- Create: `src/tests/main/http_fixture/http_fixture.cpp`
- Modify: `src/tests/main/CMakeLists.txt`

- [ ] **Step 1: Header.**

```cpp
// src/tests/main/http_fixture/http_fixture.hpp
#pragma once

#include <memory>
#include <string>
#include <filesystem>

namespace slideio { namespace tests {

class HttpFixture {
public:
    // Launches the Python server rooted at `rootDir` on an ephemeral port.
    explicit HttpFixture(const std::filesystem::path& rootDir);
    ~HttpFixture();

    int port() const { return m_port; }
    std::string url(const std::string& path) const;

    // Sets up the fixture to fail the next N GETs with 503.
    void failNextGets(int count);

    HttpFixture(const HttpFixture&) = delete;
    HttpFixture& operator=(const HttpFixture&) = delete;

private:
    int m_port = 0;
    void* m_processHandle = nullptr;
    // implementation-detail: PID on POSIX, HANDLE on Windows
};

}} // namespace
```

- [ ] **Step 2: Implementation — launch the Python script as a child process.**

For brevity, use `popen` to launch and `fgets` to read the `PORT=N` line. On Windows use `_popen` and `CreateProcessA` for proper cleanup. Concrete steps:

```cpp
// src/tests/main/http_fixture/http_fixture.cpp
#include "http_fixture.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <chrono>
#include <thread>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <signal.h>
  #include <unistd.h>
  #include <sys/wait.h>
#endif

namespace slideio { namespace tests {

#ifdef _WIN32
// Windows: spawn via CreateProcess, read stdout via anonymous pipe.
#error "Implement Windows process spawn here; mirror the POSIX path below."
#else

static std::string scriptPath() {
    // Resolved relative to the test binary's source tree.
    // Tests are run from the build directory; the python script ships in src/.
    return std::string("src/tests/main/http_fixture/test_http_server.py");
}

HttpFixture::HttpFixture(const std::filesystem::path& rootDir)
{
    int pipefd[2];
    if (pipe(pipefd) < 0) throw std::runtime_error("pipe failed");
    pid_t pid = fork();
    if (pid < 0) throw std::runtime_error("fork failed");
    if (pid == 0) {
        // child: redirect stdout to pipe, exec python
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]); close(pipefd[1]);
        execlp("python3", "python3", scriptPath().c_str(),
               "--root", rootDir.string().c_str(),
               "--port", "0",
               (char*)nullptr);
        _exit(127);
    }
    close(pipefd[1]);
    char buf[64] = {0};
    ssize_t n = ::read(pipefd[0], buf, sizeof(buf) - 1);
    close(pipefd[0]);
    if (n <= 0) throw std::runtime_error("could not read fixture port");
    std::string line(buf);
    auto eq = line.find("PORT=");
    if (eq == std::string::npos) throw std::runtime_error("fixture did not print PORT=");
    m_port = std::stoi(line.substr(eq + 5));
    m_processHandle = reinterpret_cast<void*>(static_cast<intptr_t>(pid));
    // small grace period to let server fully bind
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

HttpFixture::~HttpFixture()
{
    pid_t pid = static_cast<pid_t>(reinterpret_cast<intptr_t>(m_processHandle));
    if (pid > 0) { kill(pid, SIGTERM); int st; waitpid(pid, &st, 0); }
}

#endif

std::string HttpFixture::url(const std::string& path) const
{
    return "http://127.0.0.1:" + std::to_string(m_port) + "/" + path;
}

void HttpFixture::failNextGets(int count)
{
    std::string url = "http://127.0.0.1:" + std::to_string(m_port)
                    + "/__control__/fail-next/" + std::to_string(count);
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("failNextGets: curl_easy_init failed");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
    CURLcode rc = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);
    if (rc != CURLE_OK || code != 204) {
        throw std::runtime_error("failNextGets: control POST failed code="
                                 + std::to_string(code));
    }
}

}} // namespace
```

> **Windows note:** The `#error` block is a fail-fast placeholder. Implement Windows spawn via `CreateProcessA` with an anonymous pipe redirected to the child's `STDOUT` handle, then `ReadFile` the `PORT=` line. The exec target is `python` (not `python3`) on Windows. Tests can be left disabled on Windows until this is done; if you take that route, gate the `HttpStreamTest` suite with `#ifndef _WIN32`.

- [ ] **Step 3: CMake.**

In `src/tests/main/CMakeLists.txt`, append to `TEST_SOURCES`:

```
  http_fixture/http_fixture.cpp
```

- [ ] **Step 4: Build to confirm compilation.**

```bash
python3 install.py -a build-only -c release
```

Expected: builds (no test yet exercises the fixture; that comes in Phase E).

- [ ] **Step 5: Commit.**

```bash
git add src/tests/main/http_fixture/http_fixture.{hpp,cpp} \
        src/tests/main/CMakeLists.txt
git commit -m "Add HttpFixture C++ helper that launches the Python test server"
```

---

## Phase E — `HttpStream`

This phase is the largest. It's broken into seven small tasks so each TDD cycle exercises one behavior at a time.

### Task E1: Add libcurl Conan dependency

**Files:**
- Modify: `src/slideio/imagetools/conanfile.py`
- Modify: `src/slideio/imagetools/CMakeLists.txt`

- [ ] **Step 1: Add libcurl to Conan.**

In `src/slideio/imagetools/conanfile.py`, append inside `requirements`:

```python
        self.requires("libcurl/8.10.1")
```

- [ ] **Step 2: Update CMake.**

In `src/slideio/imagetools/CMakeLists.txt`, add:

```cmake
find_package(CURL REQUIRED)
```

and append `CURL::libcurl` to the `target_link_libraries` block for the library.

- [ ] **Step 3: Refresh Conan and rebuild.**

```bash
python3 install.py -a conan
python3 install.py -a configure-only -c release
python3 install.py -a build-only -c release
```

Expected: clean build with libcurl linked into `slideio-imagetools`.

- [ ] **Step 4: Commit.**

```bash
git add src/slideio/imagetools/conanfile.py src/slideio/imagetools/CMakeLists.txt
git commit -m "Add libcurl dependency to slideio-imagetools"
```

### Task E2: `HttpStream` skeleton + size discovery via HEAD

**Files:**
- Create: `src/slideio/imagetools/httpstream.hpp`
- Create: `src/slideio/imagetools/httpstream.cpp`
- Modify: `src/slideio/imagetools/CMakeLists.txt`
- Modify: `src/tests/main/test_httpstream.cpp` (new)

- [ ] **Step 1: Failing test — size discovery.**

```cpp
// src/tests/main/test_httpstream.cpp
#include <gtest/gtest.h>
#include "slideio/imagetools/httpstream.hpp"
#include "http_fixture/http_fixture.hpp"

#include <filesystem>
#include <fstream>
#include <string>

using slideio::tests::HttpFixture;

namespace {
std::filesystem::path makeRoot() {
    auto p = std::filesystem::temp_directory_path() / "slideio_http_fixture";
    std::filesystem::create_directories(p);
    return p;
}
}

TEST(HttpStreamTest, SizeFromHead) {
    auto root = makeRoot();
    std::filesystem::path file = root / "size.bin";
    { std::ofstream out(file, std::ios::binary); for (int i = 0; i < 12345; ++i) out.put(static_cast<char>(i & 0xff)); }
    HttpFixture fx(root);
    slideio::HttpStream s(fx.url("size.bin"));
    EXPECT_EQ(s.size(), 12345u);
}
```

Add `test_httpstream.cpp` to `TEST_SOURCES`.

- [ ] **Step 2: Header.**

```cpp
// src/slideio/imagetools/httpstream.hpp
// (license header)
#pragma once

#include "slideio/imagetools/slideio_imagetools_def.hpp"
#include "slideio/base/randomaccessstream.hpp"
#include "slideio/imagetools/blockcache.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace slideio
{
    class SLIDEIO_IMAGETOOLS_EXPORTS HttpStream : public RandomAccessStream
    {
    public:
        // Block size and cache capacity are deliberately fixed per the v1 spec.
        static constexpr size_t kBlockSize = 1u << 20;          // 1 MB
        static constexpr size_t kCacheCapacityBlocks = 256;     // 256 MB total

        explicit HttpStream(const std::string& url);
        ~HttpStream() override;

        uint64_t size() const override;
        size_t read(uint64_t offset, size_t count, void* buf) override;
        void prefetch(uint64_t offset, size_t count) override;
        std::string uri() const override;

        // Global cache toggle. Consulted on each cache lookup.
        static void setCacheEnabled(bool enabled);
        static bool cacheEnabled();

    private:
        // Returns true if the size could be discovered.
        bool probeSize();
        // Fetches blocks [firstBlock, lastBlock] in a single ranged GET. The
        // returned vector contains lastBlock-firstBlock+1 contiguous block-sized
        // chunks (final chunk may be short if it includes EOF).
        std::vector<uint8_t> fetchBlocks(uint64_t firstBlock, uint64_t lastBlock);

        std::string m_url;
        uint64_t m_size = 0;
        BlockCache m_cache;
        std::mutex m_mutex;
        static std::atomic<bool> s_cacheEnabled;
    };
}
```

- [ ] **Step 3: Implementation — HEAD probe only.**

```cpp
// src/slideio/imagetools/httpstream.cpp
// (license header)
#include "slideio/imagetools/httpstream.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/base/log.hpp"

#include <curl/curl.h>
#include <cstring>

namespace slideio
{

std::atomic<bool> HttpStream::s_cacheEnabled{true};

namespace {
size_t headerCb(char* data, size_t size, size_t nmemb, void* ud) {
    auto* sz = static_cast<uint64_t*>(ud);
    std::string h(data, size * nmemb);
    auto isPrefix = [&](const char* p) {
        size_t n = std::strlen(p);
        return h.size() >= n && ::strncasecmp(h.c_str(), p, n) == 0;
    };
    if (isPrefix("Content-Length:")) {
        *sz = std::strtoull(h.c_str() + 15, nullptr, 10);
    }
    return size * nmemb;
}
size_t discardCb(char*, size_t s, size_t n, void*) { return s * n; }
} // namespace

HttpStream::HttpStream(const std::string& url)
    : m_url(url), m_cache(kCacheCapacityBlocks)
{
    if (!probeSize()) {
        RAISE_RUNTIME_ERROR << "HttpStream: could not determine size of " << url;
    }
    SLIDEIO_LOG(INFO) << "HttpStream opened " << url << " size=" << m_size;
}

HttpStream::~HttpStream() = default;

bool HttpStream::probeSize()
{
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    uint64_t sz = 0;
    curl_easy_setopt(curl, CURLOPT_URL, m_url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &sz);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discardCb);
    CURLcode rc = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);
    if (rc != CURLE_OK || code >= 400 || sz == 0) return false;
    m_size = sz;
    return true;
}

uint64_t HttpStream::size() const { return m_size; }
std::string HttpStream::uri() const { return m_url; }

size_t HttpStream::read(uint64_t /*offset*/, size_t /*count*/, void* /*buf*/)
{
    RAISE_RUNTIME_ERROR << "HttpStream::read not implemented yet";
}

void HttpStream::prefetch(uint64_t, size_t) {}

void HttpStream::setCacheEnabled(bool enabled) { s_cacheEnabled.store(enabled); }
bool HttpStream::cacheEnabled() { return s_cacheEnabled.load(); }

std::vector<uint8_t> HttpStream::fetchBlocks(uint64_t, uint64_t)
{
    return {};  // placeholder, implemented in E4
}

} // namespace slideio
```

- [ ] **Step 4: CMake — add `httpstream.{hpp,cpp}` to `SOURCE_FILES`.**

- [ ] **Step 5: Build, run.**

```bash
python3 install.py -a build-only -c release
./build/release/bin/slideio_tests --gtest_filter="HttpStreamTest.SizeFromHead"
```

Expected: PASS.

- [ ] **Step 6: Commit.**

```bash
git add src/slideio/imagetools/httpstream.{hpp,cpp} \
        src/slideio/imagetools/CMakeLists.txt \
        src/tests/main/test_httpstream.cpp \
        src/tests/main/CMakeLists.txt
git commit -m "HttpStream: HEAD-based size discovery"
```

### Task E3: `Content-Range` fallback when `Content-Length` is missing

- [ ] **Step 1: Add a failing test that uses a server-route which omits `Content-Length` from HEAD.**

For v1 this is tested by extending the Python fixture: add a `?nohead=1` query string that makes HEAD return 200 with no Content-Length and Accept-Ranges, forcing the fallback.

Update `src/tests/main/http_fixture/test_http_server.py` HEAD handler to honor `nohead=1`:

```python
    def do_HEAD(self):
        full = self._resolve()
        if not os.path.isfile(full):
            self.send_response(404); self.end_headers(); return
        # If client passes nohead=1, return 200 with no Content-Length and no Accept-Ranges.
        if "nohead=1" in self.path:
            self.send_response(200); self.end_headers(); return
        size = os.path.getsize(full)
        self.send_response(200)
        self.send_header("Content-Length", str(size))
        self.send_header("Accept-Ranges", "bytes")
        self.end_headers()
```

Add a test:

```cpp
TEST(HttpStreamTest, SizeFromContentRangeFallback) {
    auto root = makeRoot();
    std::filesystem::path file = root / "size2.bin";
    { std::ofstream out(file, std::ios::binary); for (int i = 0; i < 999; ++i) out.put('x'); }
    HttpFixture fx(root);
    slideio::HttpStream s(fx.url("size2.bin?nohead=1"));
    EXPECT_EQ(s.size(), 999u);
}
```

- [ ] **Step 2: Implement the fallback.**

In `HttpStream::probeSize`, after the HEAD result, if `sz == 0` issue a `GET Range: bytes=0-0` and parse `Content-Range: bytes 0-0/SIZE` from the response headers:

```cpp
static size_t headerCbContentRange(char* data, size_t size, size_t nmemb, void* ud) {
    auto* sz = static_cast<uint64_t*>(ud);
    std::string h(data, size * nmemb);
    if (::strncasecmp(h.c_str(), "Content-Range:", 14) == 0) {
        auto slash = h.find('/');
        if (slash != std::string::npos) {
            *sz = std::strtoull(h.c_str() + slash + 1, nullptr, 10);
        }
    }
    return size * nmemb;
}

// inside probeSize(), after HEAD attempt:
if (m_size == 0) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    uint64_t sz = 0;
    curl_easy_setopt(curl, CURLOPT_URL, m_url.c_str());
    curl_easy_setopt(curl, CURLOPT_RANGE, "0-0");
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCbContentRange);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &sz);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discardCb);
    CURLcode rc = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);
    if (rc == CURLE_OK && code < 400 && sz > 0) { m_size = sz; return true; }
    return false;
}
return true;
```

- [ ] **Step 3: Build, run, commit.**

```bash
./build/release/bin/slideio_tests --gtest_filter="HttpStreamTest.SizeFromContentRangeFallback"
git add src/slideio/imagetools/httpstream.cpp \
        src/tests/main/test_httpstream.cpp \
        src/tests/main/http_fixture/test_http_server.py
git commit -m "HttpStream: Content-Range fallback for size discovery"
```

### Task E4: Block fetch via ranged GET + `read()` decomposition

- [ ] **Step 1: Failing tests.**

```cpp
TEST(HttpStreamTest, ReadServedFromCacheAfterFirstFetch) {
    auto root = makeRoot();
    auto file = root / "data.bin";
    std::vector<uint8_t> bytes(3 * 1024 * 1024 + 17);
    for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = uint8_t(i * 7);
    { std::ofstream out(file, std::ios::binary); out.write((char*)bytes.data(), bytes.size()); }

    HttpFixture fx(root);
    slideio::HttpStream s(fx.url("data.bin"));
    std::vector<uint8_t> buf(100);
    EXPECT_EQ(s.read(2048, buf.size(), buf.data()), buf.size());
    EXPECT_EQ(std::memcmp(buf.data(), bytes.data() + 2048, buf.size()), 0);
}

TEST(HttpStreamTest, ReadSpanningMultipleBlocks) {
    // read 2 MB starting near the end of block 0 -> spans blocks 0,1,2.
    // expectation: pixel data matches.
    // (similar shape to the test above; assertion is byte-exact)
}
```

- [ ] **Step 2: Implement `fetchBlocks` and `read`.**

```cpp
// Write-callback for ranged GET that appends into a std::vector<uint8_t>.
static size_t bodyCb(char* data, size_t size, size_t nmemb, void* ud) {
    auto* v = static_cast<std::vector<uint8_t>*>(ud);
    v->insert(v->end(), data, data + size * nmemb);
    return size * nmemb;
}

std::vector<uint8_t> HttpStream::fetchBlocks(uint64_t firstBlock, uint64_t lastBlock)
{
    uint64_t startByte = firstBlock * kBlockSize;
    uint64_t endByte = std::min((lastBlock + 1) * kBlockSize, m_size) - 1;
    std::string range = std::to_string(startByte) + "-" + std::to_string(endByte);

    std::vector<uint8_t> body;
    body.reserve(endByte - startByte + 1);

    CURL* curl = curl_easy_init();
    if (!curl) RAISE_RUNTIME_ERROR << "HttpStream: curl_easy_init failed";
    curl_easy_setopt(curl, CURLOPT_URL, m_url.c_str());
    curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, bodyCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    CURLcode rc = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);
    if (rc != CURLE_OK || (code != 200 && code != 206)) {
        RAISE_RUNTIME_ERROR << "HttpStream: fetch failed code=" << code
                            << " curl=" << curl_easy_strerror(rc);
    }
    return body;
}

size_t HttpStream::read(uint64_t offset, size_t count, void* buf)
{
    if (count == 0 || offset >= m_size) return 0;
    if (offset + count > m_size) count = static_cast<size_t>(m_size - offset);

    uint8_t* out = static_cast<uint8_t*>(buf);
    uint64_t firstBlock = offset / kBlockSize;
    uint64_t lastBlock  = (offset + count - 1) / kBlockSize;

    std::lock_guard<std::mutex> lk(m_mutex);  // serialize cache + fetch
    // Walk runs of missing blocks and fetch each run in one ranged GET.
    uint64_t b = firstBlock;
    while (b <= lastBlock) {
        if (s_cacheEnabled.load() && m_cache.contains(b)) { ++b; continue; }
        uint64_t runStart = b;
        while (b <= lastBlock && (!s_cacheEnabled.load() || !m_cache.contains(b))) ++b;
        uint64_t runEnd = b - 1;
        auto blob = fetchBlocks(runStart, runEnd);
        for (uint64_t i = runStart; i <= runEnd; ++i) {
            size_t blkOff = static_cast<size_t>((i - runStart) * kBlockSize);
            size_t blkSize = std::min<size_t>(kBlockSize, blob.size() - blkOff);
            std::vector<uint8_t> block(blob.begin() + blkOff, blob.begin() + blkOff + blkSize);
            if (s_cacheEnabled.load()) m_cache.insert(i, std::move(block));
            else {
                // not cached — copy directly into output if it overlaps
            }
        }
        // If cache is disabled, copy from `blob` directly for the slices that
        // belong to this read. Easiest: re-loop below using the cache after
        // re-inserting; or split the function. For simplicity, when cache is
        // disabled we always insert into a *temporary* cache, then `clear()` at end.
    }

    // Copy from cache (or temp store) into out.
    size_t written = 0;
    for (uint64_t i = firstBlock; i <= lastBlock; ++i) {
        std::vector<uint8_t> block;
        m_cache.get(i, block);
        size_t blockStartByte = static_cast<size_t>(i * kBlockSize);
        size_t copyFrom = (i == firstBlock) ? static_cast<size_t>(offset - blockStartByte) : 0;
        size_t remaining = count - written;
        size_t copyN = std::min(block.size() - copyFrom, remaining);
        std::memcpy(out + written, block.data() + copyFrom, copyN);
        written += copyN;
    }

    if (!s_cacheEnabled.load()) m_cache.clear();
    return written;
}
```

> **Implementer's note:** the cache-disabled branch is messy in the sketch above. Clean up to a single code path: always go through the cache for staging, but `clear()` after the read when the global toggle is off. The unit test for E7 (cache toggle) will verify "every read = one GET" via the fixture's request counter.

- [ ] **Step 3: Build, run.**

```bash
./build/release/bin/slideio_tests --gtest_filter="HttpStreamTest.ReadServedFromCacheAfterFirstFetch:HttpStreamTest.ReadSpanningMultipleBlocks"
```

- [ ] **Step 4: Commit.**

```bash
git commit -m "HttpStream: ranged-GET block fetch with run coalescing"
```

### Task E5: Add a request counter to the fixture; assert GET count

- [ ] **Step 1: Modify the Python fixture to expose `/__control__/stats` returning `{"served": N}` as plain text.**

```python
    def do_GET(self):
        if self.path.startswith("/__control__/stats"):
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            payload = f"served={Handler.served}\n".encode()
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload); return
        # ... rest unchanged
```

- [ ] **Step 2: Add `HttpFixture::servedCount()` helper that issues a one-shot GET to `/__control__/stats` and parses the count.**

- [ ] **Step 3: Failing test — coalescing.**

```cpp
TEST(HttpStreamTest, ConsecutiveBlocksCoalescedIntoOneGet) {
    auto root = makeRoot();
    auto file = root / "coalesce.bin";
    std::vector<uint8_t> bytes(5 * 1024 * 1024);
    for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = uint8_t(i);
    { std::ofstream o(file, std::ios::binary); o.write((char*)bytes.data(), bytes.size()); }

    HttpFixture fx(root);
    int before = fx.servedCount();   // ignore HEAD probe contribution
    slideio::HttpStream s(fx.url("coalesce.bin"));
    std::vector<uint8_t> buf(3 * 1024 * 1024);  // spans blocks 0,1,2 — one GET expected
    EXPECT_EQ(s.read(0, buf.size(), buf.data()), buf.size());
    int after = fx.servedCount();
    EXPECT_LE(after - before, 2);  // HEAD probe + one ranged GET (HEAD may be 0 GETs)
}
```

- [ ] **Step 4: Implementation passes already** (coalescing is implemented in E4). The test simply verifies the count.

- [ ] **Step 5: Build, run, commit.**

```bash
git commit -m "HttpStream: verify consecutive block runs coalesce into one GET"
```

### Task E6: Retry on 5xx with bounded budget

- [ ] **Step 1: Failing test.**

```cpp
TEST(HttpStreamTest, RetriesAfterTwoFiveHundredThreesThenSucceeds) {
    auto root = makeRoot();
    auto file = root / "retry.bin";
    { std::ofstream o(file, std::ios::binary); for (int i = 0; i < 4096; ++i) o.put('a'); }
    HttpFixture fx(root);
    slideio::HttpStream s(fx.url("retry.bin"));
    fx.failNextGets(2);
    std::vector<uint8_t> buf(100);
    EXPECT_EQ(s.read(0, buf.size(), buf.data()), buf.size());
}

TEST(HttpStreamTest, FailsAfterExceedingRetryBudget) {
    auto root = makeRoot();
    auto file = root / "retry2.bin";
    { std::ofstream o(file, std::ios::binary); for (int i = 0; i < 4096; ++i) o.put('a'); }
    HttpFixture fx(root);
    slideio::HttpStream s(fx.url("retry2.bin"));
    fx.failNextGets(99);
    std::vector<uint8_t> buf(100);
    EXPECT_ANY_THROW(s.read(0, buf.size(), buf.data()));
}
```

- [ ] **Step 2: Add a retry loop inside `fetchBlocks` — at most 3 attempts on `503/502/504` or transient curl errors, with exponential backoff (e.g., 50ms, 200ms, 800ms).**

Replace the `fetchBlocks` body added in E4 with the retrying version below — every retry rebuilds the curl handle from scratch (libcurl is safe to reuse via `curl_easy_reset`, but a fresh handle per attempt keeps the code simple):

```cpp
std::vector<uint8_t> HttpStream::fetchBlocks(uint64_t firstBlock, uint64_t lastBlock)
{
    uint64_t startByte = firstBlock * kBlockSize;
    uint64_t endByte = std::min((lastBlock + 1) * kBlockSize, m_size) - 1;
    std::string range = std::to_string(startByte) + "-" + std::to_string(endByte);

    constexpr int kMaxAttempts = 3;
    int delayMs = 50;
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        std::vector<uint8_t> body;
        body.reserve(endByte - startByte + 1);

        CURL* curl = curl_easy_init();
        if (!curl) RAISE_RUNTIME_ERROR << "HttpStream: curl_easy_init failed";
        curl_easy_setopt(curl, CURLOPT_URL, m_url.c_str());
        curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, bodyCb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        CURLcode rc = curl_easy_perform(curl);
        long code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        curl_easy_cleanup(curl);

        if (rc == CURLE_OK && (code == 200 || code == 206)) return body;

        bool transient = (rc == CURLE_OPERATION_TIMEDOUT) || (rc == CURLE_RECV_ERROR)
                      || (rc == CURLE_COULDNT_CONNECT) || (code >= 500 && code < 600);
        if (!transient || attempt == kMaxAttempts - 1) {
            RAISE_RUNTIME_ERROR << "HttpStream: fetch failed after " << (attempt + 1)
                                << " attempts: code=" << code
                                << " curl=" << curl_easy_strerror(rc);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        delayMs *= 4;
    }
    return {}; // unreachable
}
```

Note the requisite `#include <chrono>` and `#include <thread>` at the top of `httpstream.cpp`.

- [ ] **Step 3: Build, run, commit.**

```bash
git commit -m "HttpStream: bounded exponential-backoff retry on 5xx"
```

### Task E7: Cache toggle test

- [ ] **Step 1: Failing test.**

```cpp
TEST(HttpStreamTest, CacheDisableForcesGetPerRead) {
    auto root = makeRoot();
    auto file = root / "toggle.bin";
    { std::ofstream o(file, std::ios::binary); for (int i = 0; i < 1024 * 1024; ++i) o.put('z'); }
    HttpFixture fx(root);
    slideio::HttpStream s(fx.url("toggle.bin"));

    slideio::HttpStream::setCacheEnabled(false);
    int before = fx.servedCount();
    std::vector<uint8_t> buf(100);
    for (int i = 0; i < 5; ++i) ASSERT_EQ(s.read(0, buf.size(), buf.data()), buf.size());
    int after = fx.servedCount();
    EXPECT_GE(after - before, 5);

    slideio::HttpStream::setCacheEnabled(true);  // restore for other tests
}
```

- [ ] **Step 2: Existing implementation already consults `s_cacheEnabled.load()`. Confirm test passes.**

- [ ] **Step 3: Commit.**

```bash
git commit -m "HttpStream: cache-disable toggle test"
```

### Task E8: Plug `HttpStream` into the contract fixture

- [ ] **Step 1: Add an `HttpStreamFactory` instantiation in `test_random_access_stream_contract.cpp` that uses `HttpFixture`.** A single fixture is shared across the suite via a static instance held by the factory.

- [ ] **Step 2: Run the typed-test-suite with the HTTP factory.**

```bash
./build/release/bin/slideio_tests --gtest_filter="HttpStream/RandomAccessStreamContract*"
```

Expected: all 7 contract tests pass.

- [ ] **Step 3: Commit.**

```bash
git commit -m "HttpStream: pass shared RandomAccessStream contract tests"
```

---

## Phase F — TIFF adapter

### Task F1: `TIFFClientAdapter` — failing test

**Files:**
- Create: `src/tests/main/test_tiff_client_adapter.cpp`
- Modify: `src/tests/main/CMakeLists.txt`

- [ ] **Step 1: Add the test.** Use one of the small TIFFs already shipped in the test-data set (find any `.tif`/`.svs` under the `testlib` directory — the existing tifftools tests reference these).

```cpp
// src/tests/main/test_tiff_client_adapter.cpp
#include <gtest/gtest.h>
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/imagetools/filestream.hpp"
#include "memorystream.hpp"
#include "testlib/testlib.hpp"   // existing helper, see test_tifftools.cpp for usage

#include <fstream>
#include <vector>

TEST(TiffClientAdapterTest, ReadsSameDirectoriesAsPathOverload) {
    std::string path = TestTools::getTestImagePath("svs", "JP2K-33003-1.svs"); // adjust to any small SVS in repo
    std::vector<slideio::TiffDirectory> viaPath, viaStream;
    slideio::TiffTools::scanFile(path, viaPath);

    std::ifstream in(path, std::ios::binary | std::ios::ate);
    std::vector<uint8_t> bytes(in.tellg()); in.seekg(0); in.read((char*)bytes.data(), bytes.size());
    auto stream = std::make_shared<slideio::tests::MemoryStream>(std::move(bytes));
    auto* tiff = slideio::TiffTools::openTiffFile(stream);
    ASSERT_NE(tiff, nullptr);
    slideio::TiffTools::scanFile(tiff, viaStream);
    slideio::TiffTools::closeTiffFile(tiff);

    EXPECT_EQ(viaPath.size(), viaStream.size());
    for (size_t i = 0; i < viaPath.size(); ++i) {
        EXPECT_EQ(viaPath[i].width, viaStream[i].width);
        EXPECT_EQ(viaPath[i].height, viaStream[i].height);
        EXPECT_EQ(viaPath[i].tiled, viaStream[i].tiled);
    }
}
```

> **Test data:** if no small SVS is available, fall back to a small `.tif` already used by `test_tifftools.cpp`. The point of the test is byte-equivalence, not slide complexity.

- [ ] **Step 2: Build — should fail at link time (overload missing).**

### Task F2: Implement `TIFFClientAdapter`

**Files:**
- Create: `src/slideio/imagetools/tiffclientadapter.hpp`
- Create: `src/slideio/imagetools/tiffclientadapter.cpp`
- Modify: `src/slideio/imagetools/tifftools.hpp/.cpp`
- Modify: `src/slideio/imagetools/CMakeLists.txt`

- [ ] **Step 1: Header.**

```cpp
// src/slideio/imagetools/tiffclientadapter.hpp
// (license header)
#pragma once

#include "slideio/imagetools/slideio_imagetools_def.hpp"
#include "slideio/base/randomaccessstream.hpp"
#include "slideio/imagetools/libtiff.hpp"

#include <memory>

namespace slideio
{
    // Wraps any RandomAccessStream in libtiff's TIFFClientOpen API.
    // Returns a TIFF* that behaves identically to TIFFOpen(path)
    // for all read-only operations. The returned handle owns a
    // shared_ptr<RandomAccessStream>; closing the handle releases it.
    SLIDEIO_IMAGETOOLS_EXPORTS
    libtiff::TIFF* openTiffFromStream(std::shared_ptr<RandomAccessStream> stream);
}
```

- [ ] **Step 2: Implementation.**

```cpp
// src/slideio/imagetools/tiffclientadapter.cpp
// (license header)
#include "slideio/imagetools/tiffclientadapter.hpp"
#include "slideio/base/exceptions.hpp"

#include <tiffio.h>
#include <cstring>

namespace slideio
{

namespace {

struct TiffClientCtx {
    std::shared_ptr<RandomAccessStream> stream;
    uint64_t cursor = 0;
};

tmsize_t tiffRead(thandle_t h, void* buf, tmsize_t n) {
    auto* ctx = static_cast<TiffClientCtx*>(h);
    size_t got = ctx->stream->read(ctx->cursor, static_cast<size_t>(n), buf);
    ctx->cursor += got;
    return static_cast<tmsize_t>(got);
}
tmsize_t tiffWrite(thandle_t, void*, tmsize_t) { return -1; }
toff_t tiffSeek(thandle_t h, toff_t off, int whence) {
    auto* ctx = static_cast<TiffClientCtx*>(h);
    switch (whence) {
        case SEEK_SET: ctx->cursor = static_cast<uint64_t>(off); break;
        case SEEK_CUR: ctx->cursor += off; break;
        case SEEK_END: ctx->cursor = ctx->stream->size() + off; break;
    }
    return static_cast<toff_t>(ctx->cursor);
}
int tiffClose(thandle_t h) {
    delete static_cast<TiffClientCtx*>(h);
    return 0;
}
toff_t tiffSize(thandle_t h) {
    return static_cast<toff_t>(static_cast<TiffClientCtx*>(h)->stream->size());
}
// No memory mapping for stream-backed TIFFs.
int  tiffMap(thandle_t, void**, toff_t*) { return 0; }
void tiffUnmap(thandle_t, void*, toff_t) {}

} // namespace

libtiff::TIFF* openTiffFromStream(std::shared_ptr<RandomAccessStream> stream)
{
    if (!stream) RAISE_RUNTIME_ERROR << "openTiffFromStream: null stream";
    auto* ctx = new TiffClientCtx{ std::move(stream), 0 };
    libtiff::TIFF* t = libtiff::TIFFClientOpen(
        ctx->stream->uri().c_str(), "r", ctx,
        tiffRead, tiffWrite, tiffSeek, tiffClose, tiffSize, tiffMap, tiffUnmap);
    if (!t) {
        delete ctx;
        RAISE_RUNTIME_ERROR << "openTiffFromStream: TIFFClientOpen failed";
    }
    return t;
}

} // namespace slideio
```

- [ ] **Step 3: Add the `openTiffFile(stream)` overload to `TiffTools`.**

In `tifftools.hpp`:

```cpp
static libtiff::TIFF* openTiffFile(std::shared_ptr<RandomAccessStream> stream);
```

In `tifftools.cpp`:

```cpp
#include "slideio/imagetools/tiffclientadapter.hpp"
// ...
libtiff::TIFF* TiffTools::openTiffFile(std::shared_ptr<RandomAccessStream> stream)
{
    auto* t = openTiffFromStream(std::move(stream));
    if (!t) RAISE_RUNTIME_ERROR << "TiffTools::openTiffFile(stream) failed";
    return t;
}
```

- [ ] **Step 4: CMake — add `tiffclientadapter.{hpp,cpp}` to `SOURCE_FILES`.**

- [ ] **Step 5: Build, run.**

```bash
./build/release/bin/slideio_tests --gtest_filter="TiffClientAdapterTest*"
```

- [ ] **Step 6: Commit.**

```bash
git commit -m "Add TIFF client adapter (RandomAccessStream -> libtiff TIFF*)"
```

### Task F3: Mirror the overload in `NDPITiffTools`

- [ ] **Step 1: Add `NDPITiffTools::openTiffFile(stream)` overload in `src/slideio/drivers/ndpi/ndpitifftools.{hpp,cpp}`** that calls the same `openTiffFromStream` helper. The NDPI-specific patches in `NDPITiffTools` operate on the returned `TIFF*` and are unrelated to how the file was opened.

- [ ] **Step 2: Unit test paralleling F1 but using NDPITiffTools** (smallest NDPI sample under test data).

- [ ] **Step 3: Build, run, commit.**

```bash
git commit -m "NDPITiffTools: openTiffFile(stream) overload via TIFFClientOpen"
```

---

## Phase G — URI dispatcher

### Task G1: URI prefix detection unit tests

**Files:**
- Create: `src/tests/main/test_uri_dispatcher.cpp`
- Modify: `src/tests/main/CMakeLists.txt`

- [ ] **Step 1: Failing tests.**

```cpp
// src/tests/main/test_uri_dispatcher.cpp
#include <gtest/gtest.h>
#include "slideio/imagetools/uridispatcher.hpp"

using slideio::detectUriScheme;
using slideio::UriScheme;

TEST(UriDispatcherTest, DetectsLocalPaths) {
    EXPECT_EQ(detectUriScheme("/abs/path/file.svs"), UriScheme::LocalFile);
    EXPECT_EQ(detectUriScheme("C:\\Users\\foo\\file.svs"), UriScheme::LocalFile);
    EXPECT_EQ(detectUriScheme("relative/file.svs"), UriScheme::LocalFile);
    EXPECT_EQ(detectUriScheme("file:///abs/path/file.svs"), UriScheme::LocalFile);
}

TEST(UriDispatcherTest, DetectsS3) {
    EXPECT_EQ(detectUriScheme("s3://bucket/key/file.svs"), UriScheme::S3);
    EXPECT_EQ(detectUriScheme("S3://bucket/key.svs"), UriScheme::S3);
}

TEST(UriDispatcherTest, DetectsHttp) {
    EXPECT_EQ(detectUriScheme("http://host/p"), UriScheme::Http);
    EXPECT_EQ(detectUriScheme("https://host/p?X-Amz-Signature=abc"), UriScheme::Http);
}

TEST(UriDispatcherTest, TranslatesS3ToHttps) {
    EXPECT_EQ(slideio::s3UriToHttps("s3://mybucket/path/to/slide.svs"),
              "https://mybucket.s3.amazonaws.com/path/to/slide.svs");
}

TEST(UriDispatcherTest, CreateStreamReturnsFileStreamForPath) {
    auto path = std::filesystem::temp_directory_path() / "slideio_ud_create.bin";
    { std::ofstream out(path, std::ios::binary); out << "hello world"; }
    auto stream = slideio::createStream(path.string());
    ASSERT_NE(stream, nullptr);
    EXPECT_EQ(stream->size(), 11u);
    std::filesystem::remove(path);
}

TEST(UriDispatcherTest, UriResourceNameStripsSchemeAndQuery) {
    EXPECT_EQ(slideio::uriResourceName("s3://bucket/dir/slide.svs"), "slide.svs");
    EXPECT_EQ(slideio::uriResourceName("https://h/p/slide.svs?sig=x"), "slide.svs");
    EXPECT_EQ(slideio::uriResourceName("/abs/dir/slide.svs"), "slide.svs");
    EXPECT_EQ(slideio::uriResourceName("C:\\dir\\slide.svs"), "slide.svs");
}
```

### Task G2: Implement `UriDispatcher`

**Files:**
- Create: `src/slideio/imagetools/uridispatcher.hpp`
- Create: `src/slideio/imagetools/uridispatcher.cpp`
- Modify: `src/slideio/imagetools/CMakeLists.txt`

- [ ] **Step 1: Header.**

```cpp
// src/slideio/imagetools/uridispatcher.hpp
// (license header)
#pragma once

#include "slideio/imagetools/slideio_imagetools_def.hpp"
#include "slideio/base/randomaccessstream.hpp"

#include <memory>
#include <string>

namespace slideio
{
    enum class UriScheme { LocalFile, S3, Http };

    SLIDEIO_IMAGETOOLS_EXPORTS UriScheme detectUriScheme(const std::string& uri);

    // Translates "s3://bucket/key" -> "https://bucket.s3.amazonaws.com/key".
    // Returns the input unchanged for non-s3 URIs.
    SLIDEIO_IMAGETOOLS_EXPORTS std::string s3UriToHttps(const std::string& uri);

    // Factory: returns the appropriate RandomAccessStream for the given URI.
    SLIDEIO_IMAGETOOLS_EXPORTS std::shared_ptr<RandomAccessStream> createStream(
        const std::string& uri);

    // Returns the resource name (e.g. "slide.svs") suitable for pattern matching
    // — strips s3://, https://..., query strings, and file:// prefixes.
    SLIDEIO_IMAGETOOLS_EXPORTS std::string uriResourceName(const std::string& uri);

    // Given an originating URI and a relative or absolute name, produces a URI
    // that lives "next to" the original. Examples:
    //   siblingUri("http://h/dir/a.svs", "b.svs")        -> "http://h/dir/b.svs"
    //   siblingUri("s3://bucket/dir/a.afi", "b.svs")     -> "s3://bucket/dir/b.svs"
    //   siblingUri("/abs/dir/a.afi", "b.svs")            -> "/abs/dir/b.svs"
    // If `name` already contains a scheme, it is returned unchanged.
    SLIDEIO_IMAGETOOLS_EXPORTS std::string siblingUri(const std::string& base,
                                                      const std::string& name);
}
```

- [ ] **Step 2: Implementation.**

```cpp
// src/slideio/imagetools/uridispatcher.cpp
#include "slideio/imagetools/uridispatcher.hpp"
#include "slideio/imagetools/filestream.hpp"
#include "slideio/imagetools/httpstream.hpp"
#include "slideio/base/exceptions.hpp"

#include <algorithm>
#include <cctype>

namespace slideio {

static bool ciStartsWith(const std::string& s, const char* p) {
    size_t n = std::strlen(p);
    if (s.size() < n) return false;
    for (size_t i = 0; i < n; ++i)
        if (std::tolower(static_cast<unsigned char>(s[i])) !=
            std::tolower(static_cast<unsigned char>(p[i]))) return false;
    return true;
}

UriScheme detectUriScheme(const std::string& uri) {
    if (ciStartsWith(uri, "s3://"))   return UriScheme::S3;
    if (ciStartsWith(uri, "http://")) return UriScheme::Http;
    if (ciStartsWith(uri, "https://"))return UriScheme::Http;
    // file:// and everything else are local
    return UriScheme::LocalFile;
}

std::string s3UriToHttps(const std::string& uri) {
    if (!ciStartsWith(uri, "s3://")) return uri;
    auto rest = uri.substr(5);
    auto slash = rest.find('/');
    if (slash == std::string::npos)
        RAISE_RUNTIME_ERROR << "s3 URI missing key: " << uri;
    std::string bucket = rest.substr(0, slash);
    std::string key = rest.substr(slash + 1);
    return "https://" + bucket + ".s3.amazonaws.com/" + key;
}

static std::string stripFileScheme(const std::string& uri) {
    if (ciStartsWith(uri, "file://")) return uri.substr(7);
    return uri;
}

std::shared_ptr<RandomAccessStream> createStream(const std::string& uri) {
    switch (detectUriScheme(uri)) {
        case UriScheme::LocalFile: return std::make_shared<FileStream>(stripFileScheme(uri));
        case UriScheme::S3:        return std::make_shared<HttpStream>(s3UriToHttps(uri));
        case UriScheme::Http:      return std::make_shared<HttpStream>(uri);
    }
    RAISE_RUNTIME_ERROR << "createStream: unknown URI scheme: " << uri;
}

std::string uriResourceName(const std::string& uri) {
    std::string u = uri;
    // strip query string
    auto q = u.find('?');
    if (q != std::string::npos) u.erase(q);
    // strip schemes
    if (ciStartsWith(u, "file://"))  u.erase(0, 7);
    else if (ciStartsWith(u, "s3://"))    u.erase(0, 5);
    else if (ciStartsWith(u, "http://"))  u.erase(0, 7);
    else if (ciStartsWith(u, "https://")) u.erase(0, 8);
    // return last path segment
    auto slash = u.find_last_of("/\\");
    return (slash == std::string::npos) ? u : u.substr(slash + 1);
}

std::string siblingUri(const std::string& base, const std::string& name) {
    // If `name` already has a scheme, use it as-is.
    if (ciStartsWith(name, "s3://") || ciStartsWith(name, "http://")
        || ciStartsWith(name, "https://") || ciStartsWith(name, "file://")) {
        return name;
    }
    // Find the position after the last '/' or '\\' in `base` (after stripping query).
    std::string b = base;
    auto q = b.find('?');
    if (q != std::string::npos) b.erase(q);
    auto slash = b.find_last_of("/\\");
    if (slash == std::string::npos) return name;
    return b.substr(0, slash + 1) + name;
}

} // namespace slideio
```

- [ ] **Step 3: CMake** — add `uridispatcher.{hpp,cpp}` to `SOURCE_FILES`.

- [ ] **Step 4: Build, run.**

```bash
./build/release/bin/slideio_tests --gtest_filter="UriDispatcherTest*"
```

- [ ] **Step 5: Commit.**

```bash
git commit -m "Add URI dispatcher (createStream + scheme detection)"
```

### Task G3: `Tools::matchPattern` strips query strings

**Files:**
- Modify: `src/slideio/core/tools/tools.cpp` (or wherever `matchPattern` lives — confirm via Grep)
- Modify: `src/tests/main/test_tools.cpp`

- [ ] **Step 1: Failing test.**

```cpp
TEST(ToolsTest, MatchPatternStripsQueryString) {
    EXPECT_TRUE(slideio::Tools::matchPattern("foo.svs?X-Amz-Signature=abc", "*.svs"));
    EXPECT_TRUE(slideio::Tools::matchPattern("https://h/path/foo.svs?sig=x", "*.svs"));
}
```

- [ ] **Step 2: Modify `matchPattern` to call `uriResourceName(input)` (or inline a tiny strip) before doing the glob match.**

- [ ] **Step 3: Build, run, commit.**

```bash
git commit -m "Tools::matchPattern: strip URI query string before glob match"
```

---

## Phase H — Public-API integration

### Task H1: `ImageDriver::supportsStream()` and `openFile(stream)` virtuals

**Files:**
- Modify: `src/slideio/core/imagedriver.hpp/.cpp`

- [ ] **Step 1: Add the virtuals.**

In `src/slideio/core/imagedriver.hpp`:

```cpp
#include "slideio/base/randomaccessstream.hpp"
#include <memory>

namespace slideio
{
    class SLIDEIO_CORE_EXPORTS ImageDriver
    {
    public:
        virtual ~ImageDriver(){}
        virtual std::string getID() const = 0;
        virtual bool canOpenFile(const std::string& filePath) const;
        virtual std::shared_ptr<CVSlide> openFile(const std::string& filePath) = 0;
        virtual std::string getFileSpecs() const = 0;

        // New in v1: whether this driver can open from a RandomAccessStream
        // (rather than requiring a local-filesystem path).
        virtual bool supportsStream() const { return false; }

        // Default forwards to the path-based overload via FileStream. Drivers
        // that override supportsStream() must override this too.
        virtual std::shared_ptr<CVSlide> openFile(
            std::shared_ptr<RandomAccessStream> stream);
    };
}
```

In `src/slideio/core/imagedriver.cpp`, add a default impl that throws unless the driver overrides:

```cpp
std::shared_ptr<CVSlide> ImageDriver::openFile(
    std::shared_ptr<RandomAccessStream> /*stream*/)
{
    RAISE_RUNTIME_ERROR << "Driver " << getID()
        << " does not support stream-based open";
}
```

- [ ] **Step 2: Build — no test breaks, no caller breaks (default returns false / throws).**

- [ ] **Step 3: Commit.**

```bash
git commit -m "ImageDriver: add supportsStream() and openFile(stream) virtuals"
```

### Task H2: `ImageDriverManager::openSlide` dispatches via URI

**Files:**
- Modify: `src/slideio/slideio/imagedrivermanager.hpp/.cpp`
- Create: `src/tests/main/test_imagedrivermanager.cpp` additions

- [ ] **Step 1: Failing test — opening an `http://` URL with a not-yet-migrated driver throws a clear error.**

```cpp
TEST(ImageDriverManagerTest, RejectsHttpForDriversWithoutStreamSupport) {
    // pick the GDAL or CZI driver (not migrated in v1). Use ANY URL — driver
    // selection happens before connection.
    EXPECT_ANY_THROW(slideio::ImageDriverManager::openSlide(
        "http://example.com/foo.czi", "CZI"));
}
```

- [ ] **Step 2: Implement the dispatcher.**

```cpp
std::shared_ptr<CVSlide> ImageDriverManager::openSlide(
    const std::string& uri, const std::string& driverName)
{
    initialize();
    std::shared_ptr<slideio::ImageDriver> driver;
    if (driverName.compare("AUTO") == 0 || driverName.empty()) {
        driver = findDriver(uri);
        if (!driver.get())
            RAISE_RUNTIME_ERROR << "Cannot find driver for " << uri;
    } else {
        auto it = driverMap.find(driverName);
        if (it == driverMap.end())
            throw std::runtime_error("ImageDriverManager: Unknown driver " + driverName);
        driver = it->second;
    }

    auto scheme = detectUriScheme(uri);
    std::shared_ptr<CVSlide> slide;
    if (scheme == UriScheme::LocalFile) {
        slide = driver->openFile(uri);
    } else {
        if (!driver->supportsStream()) {
            RAISE_RUNTIME_ERROR << "Driver " << driver->getID()
                << " does not yet support remote URIs: " << uri;
        }
        slide = driver->openFile(createStream(uri));
    }
    slide->setDriverId(driver->getID());
    return slide;
}
```

Also: `findDriver(uri)` already calls `canOpenFile(uri)` which uses `matchPattern` (already updated in G3 to strip query strings). No change needed there.

- [ ] **Step 3: Build, run.**

```bash
./build/release/bin/slideio_tests --gtest_filter="ImageDriverManagerTest.RejectsHttpForDriversWithoutStreamSupport"
```

- [ ] **Step 4: Commit.**

```bash
git commit -m "ImageDriverManager: URI dispatch (path vs stream) on openSlide"
```

### Task H3: `setHttpCacheEnabled` exposed on `ImageDriverManager`

**Files:**
- Modify: `src/slideio/slideio/imagedrivermanager.hpp/.cpp`

- [ ] **Step 1: Add the static method.** It delegates to `HttpStream::setCacheEnabled`.

```cpp
// imagedrivermanager.hpp:
class ImageDriverManager {
public:
    // ...
    static void setHttpCacheEnabled(bool enabled);
};

// imagedrivermanager.cpp:
void ImageDriverManager::setHttpCacheEnabled(bool enabled) {
    HttpStream::setCacheEnabled(enabled);
}
```

- [ ] **Step 2: Add a one-liner test.**

```cpp
TEST(ImageDriverManagerTest, SetHttpCacheEnabledTogglesHttpStream) {
    slideio::ImageDriverManager::setHttpCacheEnabled(false);
    EXPECT_FALSE(slideio::HttpStream::cacheEnabled());
    slideio::ImageDriverManager::setHttpCacheEnabled(true);
    EXPECT_TRUE(slideio::HttpStream::cacheEnabled());
}
```

- [ ] **Step 3: Build, run, commit.**

```bash
git commit -m "ImageDriverManager: expose setHttpCacheEnabled"
```

---

## Phase I — Driver migrations

Each driver in this phase follows the same three-step pattern:

1. Override `supportsStream()` to return `true`.
2. Override `openFile(std::shared_ptr<RandomAccessStream> stream)` to construct the driver's `*Slide` from the stream (typically forwarding to a new static `*Slide::openFile(stream, id)` factory).
3. The slide's existing static `openFile(path, id)` becomes a thin wrapper that constructs a `FileStream` and delegates to the new stream-based factory.

Tests for each driver verify two things: (a) opening from a path still works exactly as before (existing tests must pass); (b) opening from a `MemoryStream` containing the same bytes yields a `Slide` whose scene metadata matches.

### Task I1: SVS

**Files:**
- Modify: `src/slideio/drivers/svs/svsimagedriver.hpp/.cpp`
- Modify: `src/slideio/drivers/svs/svsslide.hpp/.cpp`

- [ ] **Step 1: Failing test.**

```cpp
TEST(SVSDriverTest, OpenFromMemoryStreamMatchesPath) {
    std::string path = TestTools::getTestImagePath("svs", "JP2K-33003-1.svs");
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    std::vector<uint8_t> bytes(in.tellg()); in.seekg(0); in.read((char*)bytes.data(), bytes.size());

    auto refSlide = slideio::ImageDriverManager::openSlide(path, "SVS");
    auto stream = std::make_shared<slideio::tests::MemoryStream>(std::move(bytes), "memory:///x.svs");
    slideio::SVSImageDriver driver;
    auto streamSlide = driver.openFile(stream);

    EXPECT_EQ(refSlide->getNumScenes(), streamSlide->getNumScenes());
    EXPECT_EQ(refSlide->getScene(0)->getRect(), streamSlide->getScene(0)->getRect());
}
```

Add this test to `src/tests/main/test_svs_driver.cpp`.

- [ ] **Step 2: Add the new factory on `SVSSlide`.**

```cpp
// svsslide.hpp:
class SLIDEIO_SVS_EXPORTS SVSSlide : public slideio::CVSlide {
    // ...
public:
    static std::shared_ptr<SVSSlide> openFile(const std::string& path, const std::string& id);
    static std::shared_ptr<SVSSlide> openFile(std::shared_ptr<RandomAccessStream> stream,
                                              const std::string& id);
    // ...
};
```

In `svsslide.cpp`, refactor: the existing path-based `openFile` constructs a `FileStream` and delegates to the new stream-based factory. The stream-based factory calls `TiffTools::openTiffFile(stream)` instead of the path overload, and stores `stream->uri()` as the slide's reported `getFilePath()`.

- [ ] **Step 3: Override in `SVSImageDriver`.**

```cpp
// svsimagedriver.hpp:
class SLIDEIO_SVS_EXPORTS SVSImageDriver : public slideio::ImageDriver {
public:
    // existing API...
    bool supportsStream() const override { return true; }
    std::shared_ptr<CVSlide> openFile(std::shared_ptr<RandomAccessStream> stream) override;
};

// svsimagedriver.cpp:
std::shared_ptr<CVSlide> SVSImageDriver::openFile(std::shared_ptr<RandomAccessStream> stream) {
    return SVSSlide::openFile(stream, getID());
}
```

- [ ] **Step 4: Build, run.**

```bash
./build/release/bin/slideio_tests --gtest_filter="*SVS*"
```

Expected: existing SVS tests still pass; the new `OpenFromMemoryStreamMatchesPath` test passes.

- [ ] **Step 5: Commit.**

```bash
git commit -m "SVS driver: support RandomAccessStream-based open"
```

### Task I2: SCN

Same pattern as I1. Test file: `test_scn_driver.cpp`.

- [ ] Add `supportsStream()` override.
- [ ] Add `openFile(stream)` override delegating to `SCNSlide::openFile(stream, id)`.
- [ ] Refactor `SCNSlide::openFile(path, id)` to wrap path in `FileStream`.
- [ ] Add MemoryStream parity test.
- [ ] Build, run filter `*SCN*`, commit.

```bash
git commit -m "SCN driver: support RandomAccessStream-based open"
```

### Task I3: NDPI

Same pattern. Two extra wrinkles: (a) the driver uses `NDPITiffTools` not `TiffTools` (use the F3 overload); (b) `NDPITIFFKeeper` may need a constructor that takes the resulting `TIFF*` instead of a path.

- [ ] Add `supportsStream()` override.
- [ ] Add `openFile(stream)` override.
- [ ] Refactor `NDPIFile::openFile`/`NDPISlide::openFile` to consume a `RandomAccessStream`.
- [ ] Add MemoryStream parity test.
- [ ] Build, run filter `*NDPI*`, commit.

```bash
git commit -m "NDPI driver: support RandomAccessStream-based open"
```

### Task I4: PKE

Same pattern as SVS. Test file: `test_pke_driver.cpp` (if it exists; otherwise add minimal coverage to `test_imagetools.cpp`).

- [ ] Add overrides, parity test, commit.

```bash
git commit -m "PKE driver: support RandomAccessStream-based open"
```

### Task I5: OME-TIFF

Same pattern, plus companion-XML handling.

- [ ] Add overrides.
- [ ] Refactor companion-XML reading (currently `std::ifstream` in `otslide.cpp:131`) to use `createStream` against a URI derived from the stream's `uri()` with the basename replaced. Use a helper `siblingUri(stream->uri(), "metadata.ome.xml")` defined in `uridispatcher.cpp` if convenient.
- [ ] Add tests:
  - MemoryStream parity (matches the path-based open).
  - End-to-end OME-TIFF with external `.companion.ome` metadata over the HTTP fixture — verify the companion file is fetched from the same URI prefix.
- [ ] Build, run filter `*OmeTiff*`, commit.

```bash
git commit -m "OME-TIFF driver: support RandomAccessStream + sibling companion XML"
```

### Task I6: AFI

Same pattern, plus URI-prefix-aware resolution of referenced SVS files.

- [ ] Add overrides.
- [ ] In `afislide.cpp`, replace the path-based `getFileRelativeTo` resolution with a URI-aware helper: given the original `stream->uri()` (e.g., `http://host/case/foo.afi`) and a relative reference `foo_part1.svs`, produce `http://host/case/foo_part1.svs`. Implement this via a `siblingUri` helper in `uridispatcher.cpp`.
- [ ] Test: an AFI fixture under the HTTP fixture root that references two SVS files — verify both are opened over HTTP.
- [ ] Build, run filter `*Afi*`, commit.

```bash
git commit -m "AFI driver: support RandomAccessStream + URI-prefix reference resolution"
```

---

## Phase J — End-to-end integration

### Task J1: SVS end-to-end over HTTP fixture

**Files:**
- Create: `src/tests/main/test_s3_streaming_integration.cpp`
- Modify: `src/tests/main/CMakeLists.txt`

- [ ] **Step 1: Test.**

```cpp
// src/tests/main/test_s3_streaming_integration.cpp
#include <gtest/gtest.h>
#include "slideio/slideio/imagedrivermanager.hpp"
#include "http_fixture/http_fixture.hpp"
#include "testlib/testlib.hpp"

#include <filesystem>
#include <vector>

using slideio::tests::HttpFixture;

namespace {

std::filesystem::path stageTestFile(const std::string& driverDir, const std::string& filename) {
    auto src = std::filesystem::path(TestTools::getTestImagePath(driverDir, filename));
    auto root = std::filesystem::temp_directory_path() / "slideio_e2e";
    std::filesystem::create_directories(root);
    auto dst = root / filename;
    std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing);
    return root;
}

void compareScene(slideio::CVScene& a, slideio::CVScene& b) {
    EXPECT_EQ(a.getRect(), b.getRect());
    EXPECT_EQ(a.getNumChannels(), b.getNumChannels());
    cv::Rect r{0, 0, std::min(256, a.getRect().width), std::min(256, a.getRect().height)};
    cv::Mat ra, rb;
    a.readBlock(r, ra); b.readBlock(r, rb);
    EXPECT_EQ(cv::norm(ra, rb, cv::NORM_INF), 0);
}

} // namespace

TEST(S3StreamingIntegrationTest, SVS_PathVsHttpEquivalent) {
    auto root = stageTestFile("svs", "JP2K-33003-1.svs");
    HttpFixture fx(root);
    auto pathSlide = slideio::ImageDriverManager::openSlide(
        (root / "JP2K-33003-1.svs").string(), "SVS");
    auto httpSlide = slideio::ImageDriverManager::openSlide(
        fx.url("JP2K-33003-1.svs"), "SVS");
    ASSERT_EQ(pathSlide->getNumScenes(), httpSlide->getNumScenes());
    auto a = pathSlide->getScene(0);
    auto b = httpSlide->getScene(0);
    compareScene(*a, *b);
}
```

- [ ] **Step 2: Add to CMake.**

- [ ] **Step 3: Build, run.**

```bash
./build/release/bin/slideio_tests --gtest_filter="S3StreamingIntegrationTest.SVS*"
```

Expected: PASS.

- [ ] **Step 4: Commit.**

```bash
git commit -m "Integration: SVS-over-HTTP parity with SVS-over-path"
```

### Task J2: NDPI end-to-end

- [ ] Add the parallel `NDPI_PathVsHttpEquivalent` test with a small NDPI file from `getTestImagePath("ndpi", ...)`. Same shape as J1. Commit.

```bash
git commit -m "Integration: NDPI-over-HTTP parity"
```

### Task J3: OME-TIFF end-to-end (incl. companion XML)

- [ ] Stage both the OME-TIFF main file and its companion `.ome` / `.ome.xml` (if external) into the fixture root. Verify the slide opens over HTTP and metadata matches.

```bash
git commit -m "Integration: OME-TIFF-over-HTTP parity (with companion XML)"
```

### Task J4: AFI end-to-end

- [ ] Stage an `.afi` plus its referenced `.svs` files into the fixture root. Open `http://.../foo.afi` and assert that scene count and rect match the path-based open.

```bash
git commit -m "Integration: AFI-over-HTTP parity (referenced SVS resolved by prefix)"
```

---

## Wrap-up

### Task W1: Full test sweep + final commit

- [ ] **Step 1: Full test run.**

```bash
./build/release/bin/slideio_tests
```

Expected: 0 failures.

- [ ] **Step 2: Build all configurations.**

```bash
python3 install.py -a install -c release
python3 install.py -a install -c debug
```

- [ ] **Step 3: Verify branch state.**

```bash
git log --oneline master..s3
git status
```

Expected: ~30 commits on `s3`, clean working tree.

- [ ] **Step 4: Cross-platform sanity** — if a Linux/macOS machine is available, build there too. Document any platform-specific failures as follow-up issues.

### Task W2: Self-review against the design spec

- [ ] **Re-read `software-docs/specs/2026-05-25-s3-streaming-design.md` §1, §2, §6, §7.1, §8 and confirm each requirement is met by a task above.**

Specifically verify:
- §5: `RandomAccessStream` interface matches spec exactly (member signatures).
- §7.1: All 12 v1 work items in the spec table have at least one task.
- §8.1–8.6: 1 MB block size, 256 MB cache (`kCacheCapacityBlocks = 256`), libcurl-based, retries on 5xx, HEAD-then-`Content-Range` size discovery, mutex-serialized cache.
- §9.1: URI prefix detection matches the spec's three-row table.
- §9.2: `matchPattern` strips query string.
- §10.1: Contract tests against FileStream, MemoryStream, HttpStream.
- §10.2: TIFF adapter parity test.
- §10.3: HTTP fixture covers block-size, coalescing, eviction, cache toggle, retry, 404.

If anything is missing, add a follow-up task before declaring v1 done.

---

## Notes for the executing engineer

- **Stay on branch `s3`.** Don't merge to master until v1 is complete and reviewed.
- **Run `slideio_tests` after every task**, not just the filtered subset. C++ shared-library changes can break unrelated drivers; you want to catch it immediately.
- **Conan changes (E1) require a full `conan` step**, not just a rebuild.
- **Windows `HttpFixture` implementation (D2 step 2)** is left as a fail-fast `#error`. Either implement it via `CreateProcessA` + anonymous pipe, or gate the entire `HttpStreamTest` / `S3StreamingIntegrationTest` suites with `#ifndef _WIN32` and file a follow-up issue. Don't pretend the suite is passing on Windows when it isn't.
- **If you hit something the plan doesn't cover** — for example, a driver's existing `openFile(path)` has surprising side effects that don't carry over cleanly to the stream path — stop, document the issue in `software-docs/notes/`, and ask before improvising. The design's load-bearing assumption is "the path-based code path keeps working by wrapping in `FileStream`." If that breaks for a specific driver, it's a design issue, not an implementation issue.
- **Don't widen scope.** v2 (czi, vsi) and v3 (gdal, zvi, dcm) are explicitly out of this plan. Resist the urge to "just also do CZI" while you're in the area.
