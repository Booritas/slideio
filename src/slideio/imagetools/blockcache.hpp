// src/slideio/imagetools/blockcache.hpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
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
