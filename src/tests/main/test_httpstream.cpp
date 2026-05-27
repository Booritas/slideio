// src/tests/main/test_httpstream.cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/imagetools/httpstream.hpp"
#include "http_fixture/http_fixture.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

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
    {
        std::ofstream out(file, std::ios::binary);
        for (int i = 0; i < 12345; ++i) out.put(static_cast<char>(i & 0xff));
    }
    HttpFixture fx(root);
    slideio::HttpStream s(fx.url("size.bin"));
    EXPECT_EQ(s.size(), 12345u);
}

TEST(HttpStreamTest, SizeFromContentRangeFallback) {
    auto root = makeRoot();
    std::filesystem::path file = root / "size2.bin";
    {
        std::ofstream out(file, std::ios::binary);
        for (int i = 0; i < 999; ++i) out.put('x');
    }
    HttpFixture fx(root);
    // nohead=1 makes HEAD return 200 without Content-Length, forcing the
    // GET Range: bytes=0-0 / Content-Range fallback.
    slideio::HttpStream s(fx.url("size2.bin?nohead=1"));
    EXPECT_EQ(s.size(), 999u);
}

TEST(HttpStreamTest, ReadServedFromCacheAfterFirstFetch) {
    auto root = makeRoot();
    auto file = root / "data.bin";
    std::vector<uint8_t> bytes(3 * 1024 * 1024 + 17);
    for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<uint8_t>(i * 7);
    {
        std::ofstream out(file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

    HttpFixture fx(root);
    slideio::HttpStream s(fx.url("data.bin"));
    std::vector<uint8_t> buf(100);
    EXPECT_EQ(s.read(2048, buf.size(), buf.data()), buf.size());
    EXPECT_EQ(std::memcmp(buf.data(), bytes.data() + 2048, buf.size()), 0);

    // Second read of the same region must hit the cache and still be exact.
    std::vector<uint8_t> buf2(100);
    EXPECT_EQ(s.read(2048, buf2.size(), buf2.data()), buf2.size());
    EXPECT_EQ(std::memcmp(buf2.data(), bytes.data() + 2048, buf2.size()), 0);
}

TEST(HttpStreamTest, ReadSpanningMultipleBlocks) {
    auto root = makeRoot();
    auto file = root / "span.bin";
    std::vector<uint8_t> bytes(3 * 1024 * 1024 + 4096);
    for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<uint8_t>(i * 13 + 5);
    {
        std::ofstream out(file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

    HttpFixture fx(root);
    slideio::HttpStream s(fx.url("span.bin"));

    // Read 2 MB starting 1 KB before the end of block 0 -> spans blocks 0,1,2.
    const uint64_t offset = slideio::HttpStream::kBlockSize - 1024;
    const size_t count = 2 * 1024 * 1024;
    std::vector<uint8_t> buf(count);
    EXPECT_EQ(s.read(offset, count, buf.data()), count);
    EXPECT_EQ(std::memcmp(buf.data(), bytes.data() + offset, count), 0);
}

TEST(HttpStreamTest, ConsecutiveBlocksCoalescedIntoOneGet) {
    auto root = makeRoot();
    auto file = root / "coalesce.bin";
    std::vector<uint8_t> bytes(5 * 1024 * 1024);
    for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<uint8_t>(i);
    {
        std::ofstream o(file, std::ios::binary);
        o.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

    HttpFixture fx(root);
    // The fixture's `served` counter increments only on file GETs (do_GET),
    // not on HEAD requests (do_HEAD). The size-probe here uses a normal HEAD
    // (no nohead=1) that yields Content-Length, so it contributes zero GETs.
    slideio::HttpStream s(fx.url("coalesce.bin"));

    const int before = fx.servedCount();
    std::vector<uint8_t> buf(3 * 1024 * 1024);  // spans blocks 0,1,2
    EXPECT_EQ(s.read(0, buf.size(), buf.data()), buf.size());
    EXPECT_EQ(std::memcmp(buf.data(), bytes.data(), buf.size()), 0);
    const int after = fx.servedCount();

    // The three consecutive missing blocks must coalesce into exactly one
    // ranged GET. The HEAD probe is not a GET, so it does not affect the count.
    EXPECT_EQ(after - before, 1);
}

TEST(HttpStreamTest, RetriesAfterTwoFiveHundredThreesThenSucceeds) {
    auto root = makeRoot();
    auto file = root / "retry.bin";
    {
        std::ofstream o(file, std::ios::binary);
        for (int i = 0; i < 4096; ++i) o.put('a');
    }
    HttpFixture fx(root);
    slideio::HttpStream s(fx.url("retry.bin"));
    fx.failNextGets(2);
    std::vector<uint8_t> buf(100);
    EXPECT_EQ(s.read(0, buf.size(), buf.data()), buf.size());
}

TEST(HttpStreamTest, FailsAfterExceedingRetryBudget) {
    auto root = makeRoot();
    auto file = root / "retry2.bin";
    {
        std::ofstream o(file, std::ios::binary);
        for (int i = 0; i < 4096; ++i) o.put('a');
    }
    HttpFixture fx(root);
    slideio::HttpStream s(fx.url("retry2.bin"));
    fx.failNextGets(99);
    std::vector<uint8_t> buf(100);
    EXPECT_ANY_THROW(s.read(0, buf.size(), buf.data()));
}

TEST(HttpStreamTest, CacheDisableForcesGetPerRead) {
    auto root = makeRoot();
    auto file = root / "toggle.bin";
    {
        std::ofstream o(file, std::ios::binary);
        for (int i = 0; i < 1024 * 1024; ++i) o.put('z');
    }
    HttpFixture fx(root);
    slideio::HttpStream s(fx.url("toggle.bin"));

    slideio::HttpStream::setCacheEnabled(false);
    const int before = fx.servedCount();
    std::vector<uint8_t> buf(100);
    for (int i = 0; i < 5; ++i) {
        ASSERT_EQ(s.read(0, buf.size(), buf.data()), buf.size());
    }
    const int after = fx.servedCount();
    // Re-enable before the assertion so a failure here cannot leak the disabled
    // state into other tests (cache state is process-wide).
    slideio::HttpStream::setCacheEnabled(true);
    EXPECT_GE(after - before, 5);
}
