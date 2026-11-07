// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/mrxs/mrxs_api_def.hpp"
#include "slideio/drivers/mrxs/mrxsindex.hpp"
#include <opencv2/core.hpp>
#include <atomic>
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <tuple>
#include <vector>

namespace slideio
{
    class FileReader;
}

namespace slideio::mrxs
{
    /**@brief budgets of the mutable runtime state (design section 8).*/
    struct RuntimeOptions
    {
        size_t decodedCacheBytes = size_t(128) << 20;   ///< decoded 8-bit RGB images kept
        int maxOpenFiles = 16;                          ///< data-file handles kept open
    };

    struct RuntimeStats
    {
        int64_t decodes = 0;        ///< images decoded (cache misses)
        int64_t hits = 0;           ///< images served from the cache
        int64_t fileOpens = 0;      ///< data files opened
        int openFiles = 0;          ///< handles currently held by the cache
        size_t cachedBytes = 0;     ///< decoded bytes currently held by the cache
    };

    /**@brief the only mutable state of an open bundle: a byte-bounded LRU of decoded images
     * and a count-bounded cache of data-file handles.
     *
     * Thread-safe. No lock is held while a file is read or an image decoded, so concurrent
     * reads overlap; two threads missing on the same image may both decode it, and the
     * second insertion is dropped. Both caches hand out shared ownership, so evicting an
     * entry never invalidates one in use.*/
    class SLIDEIO_MRXS_EXPORTS MRXSRuntime
    {
    public:
        MRXSRuntime(std::vector<std::string> dataFiles, RuntimeOptions options);
        ~MRXSRuntime();
        MRXSRuntime(const MRXSRuntime&) = delete;
        MRXSRuntime& operator=(const MRXSRuntime&) = delete;

        /// The payload's bytes, through a cached handle of its data file.
        std::vector<uint8_t> payload(const PayloadRef& ref);
        /// The decoded image for a level's payload: from the cache, or from `decode`,
        /// called without any lock held. The returned image must not be modified.
        cv::Mat decoded(int level, const PayloadRef& ref, const std::function<cv::Mat()>& decode);
        RuntimeStats stats() const;

    private:
        using Key = std::tuple<int, int, int64_t, int64_t>;   // level, file, offset, length
        struct Entry
        {
            cv::Mat image;
            size_t bytes = 0;
            std::list<Key>::iterator position;
        };
        std::shared_ptr<const FileReader> file(int fileNumber);

        const std::vector<std::string> m_dataFiles;     // UTF-8 paths
        const RuntimeOptions m_options;
        mutable std::mutex m_mutex;
        std::list<Key> m_lru;                           // most recent first
        std::map<Key, Entry> m_images;
        size_t m_cachedBytes = 0;
        std::list<int> m_fileLru;                       // most recent first
        std::map<int, std::shared_ptr<const FileReader>> m_files;
        std::atomic<int64_t> m_decodes{0};
        std::atomic<int64_t> m_hits{0};
        std::atomic<int64_t> m_fileOpens{0};
    };
}
