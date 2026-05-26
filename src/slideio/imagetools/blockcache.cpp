// src/slideio/imagetools/blockcache.cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
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
