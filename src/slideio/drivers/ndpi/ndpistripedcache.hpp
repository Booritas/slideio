// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <tuple>

#include <opencv2/core.hpp>

#include "slideio/drivers/ndpi/ndpi_api_def.hpp"

namespace slideio
{
    /// Decoded SingleStripe rasters of every open NDPI file, under one process-wide
    /// byte budget.
    ///
    /// A SingleStripe directory is one JPEG that libtiff can only decode whole, so a
    /// caller walking such a level tile by tile would otherwise pay a full decode per
    /// tile. The budget is shared rather than per file because a per-file bound
    /// multiplies by the number of open slides: an application holding a hundred NDPI
    /// files open could pin a hundred budgets. Least recently used entries are evicted
    /// first, whichever file they belong to, so the level a caller is walking stays
    /// resident while ones nobody has read for a while go.
    ///
    /// Entries are keyed by a file id from newFileId() rather than by NDPIFile address:
    /// an address can be reused by the next file opened, and a stale entry under it
    /// would serve another file's pixels. An id is never reused, so the worst a stale
    /// entry can do is occupy budget until it is evicted. NDPIFile also calls
    /// releaseFile() on close, so in practice none are left behind.
    class SLIDEIO_NDPI_EXPORTS NDPIStripedRasterCache
    {
    public:
        using Decoder = std::function<cv::Mat()>;

        static NDPIStripedRasterCache& instance();
        /// An id for one open file, never reused. Also guarantees the cache is
        /// constructed before the file taking the id, which is what keeps the
        /// cache alive for that file's releaseFile() on close -- see the
        /// definition.
        static uint64_t newFileId();
        /// The most the cache holds across all files. A raster larger than this on
        /// its own is handed back and not kept.
        static constexpr size_t budgetBytes() { return 256ull * 1024ull * 1024ull; }

        /// The raster for (fileId, dirIndex, offset), decoding it with `decode` if
        /// it is not cached. The decode runs outside the cache lock: a thread asking
        /// for a raster another thread is decoding waits for that decode, and one
        /// asking for anything else is not held up by it. A decode that throws is
        /// rethrown to every thread waiting on it and is not cached. The returned
        /// Mat shares the cached buffer; callers must treat it as read-only.
        cv::Mat get(uint64_t fileId, int dirIndex, int64_t offset, const Decoder& decode);
        /// Drops every entry of one file. Called by NDPIFile on close.
        void releaseFile(uint64_t fileId);
        /// Bytes currently held across all files. For tests.
        size_t cachedBytes() const;

    private:
        NDPIStripedRasterCache() = default;
        using Key = std::tuple<uint64_t, int, int64_t>;
        struct Entry
        {
            std::shared_future<cv::Mat> raster;
            // 0 while the raster is still being decoded. Such an entry is never
            // evicted: its future is what concurrent readers are waiting on.
            size_t bytes = 0;
            uint64_t lastUse = 0;
        };
        void evictOverBudget(const Key& keep);

        mutable std::mutex m_mutex;
        std::map<Key, Entry> m_entries;
        size_t m_bytes = 0;
        uint64_t m_clock = 0;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
