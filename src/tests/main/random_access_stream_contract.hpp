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
