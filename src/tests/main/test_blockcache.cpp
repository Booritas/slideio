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
