// src/slideio/imagetools/httpstream.hpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/imagetools/slideio_imagetools_def.hpp"
#include "slideio/base/randomaccessstream.hpp"
#include "slideio/imagetools/blockcache.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace slideio
{
    class SLIDEIO_IMAGETOOLS_EXPORTS HttpStream : public RandomAccessStream
    {
    public:
        // Block size and cache capacity are deliberately fixed per the v1 spec.
        static constexpr size_t kBlockSize = 1u << 20;          // 1 MB
        static constexpr size_t kCacheCapacityBlocks = 256;     // 256 MB total

        explicit HttpStream(const std::string& url);
        ~HttpStream() override;

        HttpStream(const HttpStream&) = delete;
        HttpStream& operator=(const HttpStream&) = delete;

        uint64_t size() const override;
        size_t read(uint64_t offset, size_t count, void* buf) override;
        void prefetch(uint64_t offset, size_t count) override;
        std::string uri() const override;

        // Global cache toggle. Consulted on each cache lookup.
        static void setCacheEnabled(bool enabled);
        static bool cacheEnabled();

    private:
        // Returns true if the size could be discovered.
        bool probeSize();
        // Fetches blocks [firstBlock, lastBlock] in a single ranged GET.
        std::vector<uint8_t> fetchBlocks(uint64_t firstBlock, uint64_t lastBlock);

        std::string m_url;
        uint64_t m_size = 0;
        BlockCache m_cache;
        std::mutex m_mutex;
        static std::atomic<bool> s_cacheEnabled;
    };
}
