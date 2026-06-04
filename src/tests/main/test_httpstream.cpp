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

TEST(HttpStreamTest, SizeDiscoveredOnOpen) {
    auto root = makeRoot();
    std::filesystem::path file = root / "size.bin";
    {
        std::ofstream out(file, std::ios::binary);
        for (int i = 0; i < 12345; ++i) out.put(static_cast<char>(i & 0xff));
    }
    HttpFixture fx(root);
    // Open issues a single ranged GET; the size is read from its Content-Range
    // total. The requested range (0..blockSize-1) overshoots this small file and
    // is clamped to EOF by the server, as S3 does.
    slideio::HttpStream s(fx.url("size.bin"));
    EXPECT_EQ(s.size(), 12345u);
}

TEST(HttpStreamTest, SizeDiscoveredForSubBlockFile) {
    auto root = makeRoot();
    std::filesystem::path file = root / "size2.bin";
    {
        std::ofstream out(file, std::ios::binary);
        for (int i = 0; i < 999; ++i) out.put('x');
    }
    HttpFixture fx(root);
    // A file far smaller than one block still yields its exact size from the
    // clamped 206's Content-Range header.
    slideio::HttpStream s(fx.url("size2.bin"));
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
    slideio::HttpStream s(fx.url("coalesce.bin"));

    // Open primes block 0 with one GET. Measuring GETs only from here on, the
    // 3 MB read spans blocks 0,1,2: block 0 is already cached, so blocks 1 and 2
    // are the only missing ones and must coalesce into a single ranged GET.
    const int before = fx.servedCount();
    std::vector<uint8_t> buf(3 * 1024 * 1024);  // spans blocks 0,1,2
    EXPECT_EQ(s.read(0, buf.size(), buf.data()), buf.size());
    EXPECT_EQ(std::memcmp(buf.data(), bytes.data(), buf.size()), 0);
    const int after = fx.servedCount();

    EXPECT_EQ(after - before, 1);
}

TEST(HttpStreamTest, OpenPrimesFirstBlockWithSingleGet) {
    auto root = makeRoot();
    auto file = root / "prime.bin";
    std::vector<uint8_t> bytes(2 * 1024 * 1024 + 512);  // > 1 block
    for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<uint8_t>(i * 17 + 3);
    {
        std::ofstream o(file, std::ios::binary);
        o.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

    HttpFixture fx(root);
    slideio::HttpStream s(fx.url("prime.bin"));

    // Opening discovers the size and fetches the first block in a SINGLE ranged
    // GET (no standalone HEAD). That one GET is the only request so far.
    EXPECT_EQ(s.size(), bytes.size());
    EXPECT_EQ(fx.servedCount(), 1);

    // A read fully inside the first block is served from that primed cache block
    // without any further request.
    std::vector<uint8_t> buf(100);
    EXPECT_EQ(s.read(1000, buf.size(), buf.data()), buf.size());
    EXPECT_EQ(std::memcmp(buf.data(), bytes.data() + 1000, buf.size()), 0);
    EXPECT_EQ(fx.servedCount(), 1);
}

TEST(HttpStreamTest, ReusesSingleConnectionAcrossReads) {
    auto root = makeRoot();
    auto file = root / "reuse.bin";
    std::vector<uint8_t> bytes(3 * 1024 * 1024 + 4096);  // > 3 blocks
    for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<uint8_t>(i * 31 + 7);
    {
        std::ofstream o(file, std::ios::binary);
        o.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

    HttpFixture fx(root);
    slideio::HttpStream s(fx.url("reuse.bin"));

    // Three reads of distinct, uncached blocks -> three separate ranged GETs.
    // With HTTP keep-alive and a reused curl handle they must all travel over a
    // single TCP connection.
    std::vector<uint8_t> buf(100);
    for (uint64_t blk = 0; blk < 3; ++blk) {
        const uint64_t off = blk * slideio::HttpStream::kBlockSize;
        ASSERT_EQ(s.read(off, buf.size(), buf.data()), buf.size());
        ASSERT_EQ(std::memcmp(buf.data(), bytes.data() + off, buf.size()), 0);
    }

    EXPECT_EQ(fx.connectionCount(), 1);
}

TEST(HttpStreamTest, RetriesAfterTwoFiveHundredThreesThenSucceeds) {
    auto root = makeRoot();
    auto file = root / "retry.bin";
    // > 1 block so the read below targets block 1, which open does NOT prime
    // (open caches only block 0). The injected 503s are thus exercised by a real
    // fetch rather than short-circuited by the primed first block.
    const uint64_t off = slideio::HttpStream::kBlockSize;
    {
        std::ofstream o(file, std::ios::binary);
        for (uint64_t i = 0; i < off + 4096; ++i) o.put('a');
    }
    HttpFixture fx(root);
    slideio::HttpStream s(fx.url("retry.bin"));
    fx.failNextGets(2);
    std::vector<uint8_t> buf(100);
    EXPECT_EQ(s.read(off, buf.size(), buf.data()), buf.size());
}

TEST(HttpStreamTest, FailsAfterExceedingRetryBudget) {
    auto root = makeRoot();
    auto file = root / "retry2.bin";
    // Read block 1 (not primed by open) so the fetch actually hits the server
    // and exhausts the retry budget.
    const uint64_t off = slideio::HttpStream::kBlockSize;
    {
        std::ofstream o(file, std::ios::binary);
        for (uint64_t i = 0; i < off + 4096; ++i) o.put('a');
    }
    HttpFixture fx(root);
    slideio::HttpStream s(fx.url("retry2.bin"));
    fx.failNextGets(99);
    std::vector<uint8_t> buf(100);
    EXPECT_ANY_THROW(s.read(off, buf.size(), buf.data()));
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

TEST(HttpStreamTest, RejectsTwoHundredForRangedRequestAtNonzeroOffset) {
    auto root = makeRoot();
    auto file = root / "ignore_range.bin";
    // > 1 block so a read at offset 1 MB produces a ranged GET starting past 0.
    const size_t fileSize = slideio::HttpStream::kBlockSize + 4096;
    {
        std::ofstream o(file, std::ios::binary);
        for (size_t i = 0; i < fileSize; ++i) o.put(static_cast<char>(i & 0xff));
    }
    HttpFixture fx(root);
    // HEAD (size probe) ignores the query and returns a correct Content-Length;
    // the GET sees ignore_range=1 and returns 200 with the full body instead of
    // a 206 partial. A read in block 1 starts at byte 1 MB > 0, so the stream
    // must reject the mis-sliceable 200 rather than serve wrong data.
    slideio::HttpStream s(fx.url("ignore_range.bin?ignore_range=1"));
    std::vector<uint8_t> buf(100);
    EXPECT_ANY_THROW(s.read(slideio::HttpStream::kBlockSize, buf.size(), buf.data()));
}
