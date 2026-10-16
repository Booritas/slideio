// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/ndpi/ndpistripedcache.hpp"

#include <atomic>
#include <exception>

slideio::NDPIStripedRasterCache& slideio::NDPIStripedRasterCache::instance()
{
    static NDPIStripedRasterCache cache;
    return cache;
}

uint64_t slideio::NDPIStripedRasterCache::newFileId()
{
    // Touches the singleton for its side effect, which is the whole point of the
    // line: every NDPIFile takes an id when it is constructed and calls
    // releaseFile() when it is destroyed, so the cache must outlive every file.
    //
    // Function-local statics are destroyed in reverse order of construction, and
    // without this the cache is first constructed by the first striped read --
    // after the file that performs it. A file in static storage, which the Python
    // bindings make easy to end up with, would then be destroyed AFTER the cache,
    // and releaseFile() would lock a destroyed mutex and walk a destroyed map.
    // Constructing the cache here, from the file's own member initialiser, makes
    // its construction complete first and so its destruction last.
    //
    // Not covered by a test: a flag saying "the cache exists" is already true by
    // the time any test but the first one runs, so such a test would pass whether
    // or not this line is here. Covering it honestly needs a separate process
    // holding a slide in static storage.
    instance();
    static std::atomic<uint64_t> next{1};
    return next.fetch_add(1);
}

cv::Mat slideio::NDPIStripedRasterCache::get(uint64_t fileId, int dirIndex, int64_t offset, const Decoder& decode)
{
    const Key key(fileId, dirIndex, offset);
    std::promise<cv::Mat> promise;
    std::shared_future<cv::Mat> pending;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_entries.find(key);
        if (it != m_entries.end()) {
            it->second.lastUse = ++m_clock;
            pending = it->second.raster;
        }
        else {
            Entry entry;
            entry.raster = promise.get_future().share();
            entry.lastUse = ++m_clock;
            m_entries.emplace(key, entry);
        }
    }
    if (pending.valid()) {
        // Cached, or being decoded by another thread: wait for that decode rather
        // than starting a second one. Rethrows if that decode failed.
        return pending.get();
    }

    // This thread owns the decode, and runs it without the lock.
    cv::Mat raster;
    try {
        raster = decode();
    }
    catch (...) {
        promise.set_exception(std::current_exception());
        std::lock_guard<std::mutex> lock(m_mutex);
        // Forget the failure so a later read tries again.
        m_entries.erase(key);
        throw;
    }
    promise.set_value(raster);

    const size_t bytes = raster.total() * raster.elemSize();
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_entries.find(key);
    if (it == m_entries.end()) {
        // The file was released while this decode ran.
        return raster;
    }
    if (raster.empty() || bytes > budgetBytes()) {
        // Too large to keep. Threads already waiting have their copy; nothing else
        // is evicted to make room for an entry that is not stored.
        m_entries.erase(it);
        return raster;
    }
    it->second.bytes = bytes;
    m_bytes += bytes;
    evictOverBudget(key);
    return raster;
}

void slideio::NDPIStripedRasterCache::evictOverBudget(const Key& keep)
{
    while (m_bytes > budgetBytes()) {
        // The least recently used finished entry other than the one just stored.
        // One always exists while over budget, since that entry alone fits.
        auto victim = m_entries.end();
        for (auto cur = m_entries.begin(); cur != m_entries.end(); ++cur) {
            if (cur->first == keep || cur->second.bytes == 0) {
                continue;
            }
            if (victim == m_entries.end() || cur->second.lastUse < victim->second.lastUse) {
                victim = cur;
            }
        }
        if (victim == m_entries.end()) {
            break;
        }
        m_bytes -= victim->second.bytes;
        m_entries.erase(victim);
    }
}

void slideio::NDPIStripedRasterCache::releaseFile(uint64_t fileId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_entries.begin(); it != m_entries.end();) {
        if (std::get<0>(it->first) == fileId) {
            m_bytes -= it->second.bytes;
            it = m_entries.erase(it);
        }
        else {
            ++it;
        }
    }
}

size_t slideio::NDPIStripedRasterCache::cachedBytes() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_bytes;
}
