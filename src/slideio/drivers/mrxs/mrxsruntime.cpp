// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/mrxs/mrxsruntime.hpp"
#include "slideio/core/tools/filereader.hpp"

using namespace slideio::mrxs;

MRXSRuntime::MRXSRuntime(std::vector<std::string> dataFiles, RuntimeOptions options)
    : m_dataFiles(std::move(dataFiles)), m_options(options)
{
}

MRXSRuntime::~MRXSRuntime() = default;

std::shared_ptr<const slideio::FileReader> MRXSRuntime::file(int fileNumber)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto it = m_files.find(fileNumber);
        if (it != m_files.end()) {
            m_fileLru.remove(fileNumber);
            m_fileLru.push_front(fileNumber);
            return it->second;
        }
    }
    // Opened without the lock; a thread racing to open the same file keeps whichever
    // handle reached the cache first and drops its own.
    auto reader = std::make_shared<const FileReader>(m_dataFiles.at(fileNumber));
    ++m_fileOpens;
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_files.find(fileNumber);
    if (it != m_files.end()) {
        return it->second;
    }
    if (m_options.maxOpenFiles > 0) {
        while (static_cast<int>(m_files.size()) >= m_options.maxOpenFiles) {
            // A reader still using an evicted handle keeps it alive through its shared_ptr.
            m_files.erase(m_fileLru.back());
            m_fileLru.pop_back();
        }
        m_files.emplace(fileNumber, reader);
        m_fileLru.push_front(fileNumber);
    }
    return reader;
}

std::vector<uint8_t> MRXSRuntime::payload(const PayloadRef& ref)
{
    const std::shared_ptr<const FileReader> reader = file(ref.fileNumber);
    std::vector<uint8_t> data(static_cast<size_t>(ref.length));
    reader->readAt(static_cast<uint64_t>(ref.offset), data.data(), data.size());
    return data;
}

cv::Mat MRXSRuntime::decoded(int level, const PayloadRef& ref, const std::function<cv::Mat()>& decode)
{
    const Key key{level, ref.fileNumber, ref.offset, ref.length};
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto it = m_images.find(key);
        if (it != m_images.end()) {
            m_lru.splice(m_lru.begin(), m_lru, it->second.position);
            ++m_hits;
            return it->second.image;
        }
    }
    // Decoded without the lock. A failure throws here and leaves nothing in the cache.
    cv::Mat image = decode();
    ++m_decodes;
    const size_t bytes = image.total() * image.elemSize();
    if (bytes == 0 || bytes > m_options.decodedCacheBytes) {
        return image;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_images.count(key)) {
        return image;       // another thread decoded and inserted it meanwhile
    }
    while (m_cachedBytes + bytes > m_options.decodedCacheBytes && !m_lru.empty()) {
        const auto victim = m_images.find(m_lru.back());
        m_cachedBytes -= victim->second.bytes;
        m_images.erase(victim);     // a reader holding the cv::Mat keeps its pixels
        m_lru.pop_back();
    }
    m_lru.push_front(key);
    m_images.emplace(key, Entry{image, bytes, m_lru.begin()});
    m_cachedBytes += bytes;
    return image;
}

void MRXSRuntime::noteRendered(int64_t pixels)
{
    int64_t peak = m_peakRenderedPixels.load();
    while (pixels > peak && !m_peakRenderedPixels.compare_exchange_weak(peak, pixels)) {
    }
}

RuntimeStats MRXSRuntime::stats() const
{
    RuntimeStats s;
    s.decodes = m_decodes.load();
    s.hits = m_hits.load();
    s.fileOpens = m_fileOpens.load();
    s.peakRenderedPixels = m_peakRenderedPixels.load();
    std::lock_guard<std::mutex> lock(m_mutex);
    s.openFiles = static_cast<int>(m_files.size());
    s.cachedBytes = m_cachedBytes;
    return s;
}
