// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/core/tools/filereader.hpp"
#include "slideio/core/tools/sequentialreader.hpp"
#include "slideio/core/exceptions.hpp"
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <thread>
#include <vector>

namespace
{
    // Writes a temp file of `size` bytes where byte i == i % 251, so any offset
    // has a locally checkable expected value.
    std::string writePattern(const std::string& name, size_t size) {
        const std::filesystem::path path =
            std::filesystem::temp_directory_path() / name;
        std::vector<uint8_t> data(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint8_t>(i % 251);
        }
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(data.data()), data.size());
        out.close();
        return path.string();
    }
}

TEST(FileReader, reportsSize) {
    const std::string path = writePattern("slideio_fr_size.bin", 4096);
    slideio::FileReader reader(path);
    EXPECT_EQ(reader.size(), 4096u);
    std::filesystem::remove(path);
}

TEST(FileReader, readsAtAnOffset) {
    const std::string path = writePattern("slideio_fr_offset.bin", 4096);
    slideio::FileReader reader(path);
    std::vector<uint8_t> buffer(16);
    reader.readAt(1000, buffer.data(), buffer.size());
    for (size_t i = 0; i < buffer.size(); ++i) {
        EXPECT_EQ(buffer[i], static_cast<uint8_t>((1000 + i) % 251)) << "byte " << i;
    }
    std::filesystem::remove(path);
}

// The property the whole class exists for: no shared cursor. Two sequential
// reads at the same offset must return the same bytes, and a read must not
// depend on what was read before it.
TEST(FileReader, readsAreIndependentOfOrder) {
    const std::string path = writePattern("slideio_fr_order.bin", 8192);
    slideio::FileReader reader(path);
    std::vector<uint8_t> first(32), second(32);
    reader.readAt(4096, first.data(), first.size());
    reader.readAt(0, second.data(), second.size());
    std::vector<uint8_t> again(32);
    reader.readAt(4096, again.data(), again.size());
    EXPECT_EQ(first, again);
    std::filesystem::remove(path);
}

TEST(FileReader, throwsPastEndOfFile) {
    const std::string path = writePattern("slideio_fr_eof.bin", 512);
    slideio::FileReader reader(path);
    std::vector<uint8_t> buffer(256);
    EXPECT_THROW(reader.readAt(400, buffer.data(), buffer.size()),
                 slideio::RuntimeError);
    std::filesystem::remove(path);
}

TEST(FileReader, throwsOnMissingFile) {
    EXPECT_THROW(slideio::FileReader("no-such-file-4b8c1e.bin"),
                 slideio::RuntimeError);
}

// 16 threads reading overlapping offsets through ONE FileReader. Every read is
// checked against the byte pattern, so a shared-cursor bug shows up as wrong
// data rather than as a crash. This is also the test the ThreadSanitizer job in
// Task 7 runs.
TEST(FileReader, concurrentReadsReturnCorrectBytes) {
    const std::string path = writePattern("slideio_fr_mt.bin", 1u << 20);
    slideio::FileReader reader(path);
    constexpr int threadCount = 16;
    constexpr int readsPerThread = 200;
    std::vector<std::thread> threads;
    std::atomic<int> failures{0};
    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back([&reader, &failures, t, readsPerThread]() {
            std::vector<uint8_t> buffer(64);
            for (int i = 0; i < readsPerThread; ++i) {
                const uint64_t offset =
                    static_cast<uint64_t>((t * 7919 + i * 4093) % ((1 << 20) - 64));
                reader.readAt(offset, buffer.data(), buffer.size());
                for (size_t b = 0; b < buffer.size(); ++b) {
                    if (buffer[b] != static_cast<uint8_t>((offset + b) % 251)) {
                        ++failures;
                        return;
                    }
                }
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    EXPECT_EQ(failures.load(), 0);
    std::filesystem::remove(path);
}

// Spec §6's short-read gate. pread is allowed to return fewer bytes than asked
// for; ifstream::read hid that, so the retry loop is the one guarantee actively
// lost in this migration. A filesystem that produces short reads on demand is
// not portable to arrange, so the loop is tested directly through its seam.
TEST(FileReader, retriesShortReadsUntilSatisfied) {
    std::vector<uint8_t> destination(1000, 0);
    int calls = 0;
    // Returns one byte per call, so a loop that trusts the first return value
    // fills 1 byte out of 1000 and this test catches it.
    slideio::FileReader::ReadPrimitive dribble =
        [&calls](void* dst, size_t size, uint64_t offset) -> int64_t {
            ++calls;
            if (size == 0) {
                return 0;
            }
            *static_cast<uint8_t*>(dst) = static_cast<uint8_t>(offset % 251);
            return 1;
        };
    slideio::FileReader::fillFrom(destination.data(), destination.size(), 7,
                                  dribble, "fake");
    EXPECT_EQ(calls, 1000);
    for (size_t i = 0; i < destination.size(); ++i) {
        EXPECT_EQ(destination[i], static_cast<uint8_t>((7 + i) % 251)) << "byte " << i;
    }
}

TEST(FileReader, retriesInterruptedReads) {
    std::vector<uint8_t> destination(4, 0);
    int calls = 0;
    // -1 means "retryable interruption" (EINTR). The loop must not treat it as
    // an error and must not advance the offset.
    slideio::FileReader::ReadPrimitive flaky =
        [&calls](void* dst, size_t size, uint64_t offset) -> int64_t {
            ++calls;
            if (calls % 2 == 1) {
                return -1;
            }
            *static_cast<uint8_t*>(dst) = 0xAB;
            return 1;
        };
    slideio::FileReader::fillFrom(destination.data(), destination.size(), 0,
                                  flaky, "fake");
    EXPECT_EQ(calls, 8);
    EXPECT_EQ(destination, std::vector<uint8_t>(4, 0xAB));
}

TEST(FileReader, throwsWhenThePrimitiveHitsEndOfFile) {
    std::vector<uint8_t> destination(8, 0);
    slideio::FileReader::ReadPrimitive empty =
        [](void*, size_t, uint64_t) -> int64_t { return 0; };
    EXPECT_THROW(slideio::FileReader::fillFrom(destination.data(), destination.size(),
                                               0, empty, "fake"),
                 slideio::RuntimeError);
}

TEST(SequentialReader, readsStructsAndSkips) {
    const std::string path = writePattern("slideio_sr.bin", 1024);
    slideio::FileReader reader(path);
    slideio::SequentialReader sequential(reader);
    uint32_t first = 0;
    sequential.read(first);
    EXPECT_EQ(sequential.pos(), 4u);
    sequential.skip(4);
    EXPECT_EQ(sequential.pos(), 8u);
    uint8_t byte = 0;
    sequential.read(byte);
    EXPECT_EQ(byte, static_cast<uint8_t>(8));
    std::filesystem::remove(path);
}
