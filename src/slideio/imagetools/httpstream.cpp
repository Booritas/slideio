// src/slideio/imagetools/httpstream.cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/imagetools/httpstream.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/base/log.hpp"

#include <curl/curl.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <thread>

#ifdef _WIN32
  #define SLIDEIO_STRNCASECMP _strnicmp
#else
  #include <strings.h>
  #define SLIDEIO_STRNCASECMP strncasecmp
#endif

namespace slideio
{

std::atomic<bool> HttpStream::s_cacheEnabled{true};

namespace {

std::once_flag g_curlInitFlag;
void ensureCurlGlobalInit() {
    std::call_once(g_curlInitFlag, []() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    });
}

// Captures the object size from a size-probe response. A ranged GET yields a
// "Content-Range: bytes START-END/TOTAL" header (preferred); a server that
// ignores the Range and replies 200 yields a "Content-Length: TOTAL". Both are
// parsed so the probe works against either.
struct ProbeSize { uint64_t total = 0; uint64_t length = 0; };

size_t probeHeaderCb(char* data, size_t size, size_t nmemb, void* ud) {
    auto* p = static_cast<ProbeSize*>(ud);
    const std::string h(data, size * nmemb);
    auto matches = [&](const char* prefix) {
        const size_t n = std::strlen(prefix);
        return h.size() >= n && SLIDEIO_STRNCASECMP(h.c_str(), prefix, n) == 0;
    };
    if (matches("Content-Range:")) {
        const auto slash = h.find('/');
        if (slash != std::string::npos) {
            p->total = std::strtoull(h.c_str() + slash + 1, nullptr, 10);
        }
    } else if (matches("Content-Length:")) {
        p->length = std::strtoull(h.c_str() + std::strlen("Content-Length:"),
                                  nullptr, 10);
    }
    return size * nmemb;
}

// Write-callback for ranged GET that appends the body into a std::vector<uint8_t>.
size_t bodyCb(char* data, size_t size, size_t nmemb, void* ud) {
    auto* v = static_cast<std::vector<uint8_t>*>(ud);
    v->insert(v->end(), data, data + size * nmemb);
    return size * nmemb;
}

// Builds a single-line snippet of a captured response body, suitable for
// embedding in an error message. AWS S3 returns XML like
// <Error><Code>SignatureDoesNotMatch</Code><Message>...</Message></Error>
// on 4xx; surfacing it tells the user WHICH 403 they hit.
std::string responseBodySnippet(const std::vector<uint8_t>& body) {
    if (body.empty()) return {};
    const size_t n = std::min<size_t>(body.size(), 2048);
    std::string s(body.begin(), body.begin() + n);
    for (auto& c : s) {
        if (c == '\r' || c == '\n' || c == '\t') c = ' ';
    }
    return s;
}

// A transient failure is a timeout, a dropped connection, a connect failure, or
// any HTTP 5xx response -- the kinds of errors that a retry can plausibly fix.
bool isTransient(CURLcode rc, long httpCode) {
    return (rc == CURLE_OPERATION_TIMEDOUT)
        || (rc == CURLE_RECV_ERROR)
        || (rc == CURLE_COULDNT_CONNECT)
        || (httpCode >= 500 && httpCode < 600);
}

// Performs an HTTP request with a bounded, exponential-backoff retry policy
// (3 attempts; 50, 200, 800ms between attempts). Each attempt resets the
// supplied persistent `curl` handle to a clean option state, hands it to
// `configure` (which sets URL/method/callbacks), and performs it. The handle is
// NOT destroyed between requests: curl_easy_reset preserves live connections,
// the DNS cache and the TLS session cache, so subsequent requests to the same
// host reuse the existing TCP+TLS connection (HTTP keep-alive). `accept` decides
// whether the (rc, httpCode) pair is a success; on success this returns
// normally. Transient failures are retried; a non-transient failure, or
// exhausting the budget, throws via RAISE_RUNTIME_ERROR with `context` in the
// message. Returns the last (rc, httpCode) on success.
struct RequestResult { CURLcode rc; long httpCode; };

RequestResult performWithRetry(CURL* curl,
                               const std::function<void(CURL*)>& configure,
                               const std::function<bool(CURLcode, long)>& accept,
                               const std::string& context) {
    constexpr int kMaxAttempts = 3;
    int delayMs = 50;
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        // Clear options from the previous request (e.g. a stale CURLOPT_RANGE)
        // while keeping the connection/session caches that enable reuse.
        curl_easy_reset(curl);
        configure(curl);
        CURLcode rc = curl_easy_perform(curl);
        long code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);

        if (accept(rc, code)) {
            return {rc, code};
        }
        if (!isTransient(rc, code) || attempt == kMaxAttempts - 1) {
            RAISE_RUNTIME_ERROR << "HttpStream: " << context << " failed after "
                                << (attempt + 1) << " attempts: code=" << code
                                << " curl=" << curl_easy_strerror(rc);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        delayMs *= 4;
    }
    return {CURLE_OK, 0}; // unreachable
}

} // namespace

HttpStream::HttpStream(const std::string& url)
    : m_url(url), m_cache(kCacheCapacityBlocks)
{
    ensureCurlGlobalInit();
    m_curl = curl_easy_init();
    if (!m_curl) RAISE_RUNTIME_ERROR << "HttpStream: curl_easy_init failed";
    if (!probeSize()) {
        RAISE_RUNTIME_ERROR << "HttpStream: could not determine size of " << url;
    }
    SLIDEIO_LOG(INFO) << "HttpStream opened " << url << " size=" << m_size;
}

HttpStream::~HttpStream()
{
    if (m_curl) {
        curl_easy_cleanup(static_cast<CURL*>(m_curl));
        m_curl = nullptr;
    }
}

bool HttpStream::probeSize()
{
    // TODO(spec §8.4): verify Accept-Ranges: bytes at construction and fail fast
    // if the server does not advertise range support.

    // Single ranged GET that both discovers the object size AND primes the first
    // block, folding what used to be a separate HEAD (or HEAD + Content-Range
    // fallback) into the same round-trip that reads the file header. This:
    //   * removes one round-trip from every open (no standalone HEAD), which
    //     dominates latency for high-RTT S3 streams; and
    //   * works with AWS S3 presigned URLs, which are signed for a single method
    //     and reject HEAD with 403 (the AWS CLI signs them for GET).
    // The size comes from the 206's Content-Range "/TOTAL"; a non-conforming
    // server that ignores Range and returns 200 still yields Content-Length.
    const std::string range = "0-" + std::to_string(kBlockSize - 1);
    ProbeSize probe;
    std::vector<uint8_t> body;
    try {
        performWithRetry(
            static_cast<CURL*>(m_curl),
            [&](CURL* curl) {
                // body is appended to across attempts; reset so a failed attempt
                // does not contaminate the next one's response.
                body.clear();
                curl_easy_setopt(curl, CURLOPT_URL, m_url.c_str());
                curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str());
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
                // On Windows, Conan's libcurl/OpenSSL builds ship without a default
                // CA bundle path; without NATIVE_CA the Windows certificate store is
                // ignored and every HTTPS peer fails verification.
                curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
                curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, probeHeaderCb);
                curl_easy_setopt(curl, CURLOPT_HEADERDATA, &probe);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, bodyCb);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
            },
            [](CURLcode rc, long code) {
                // Both 206 (ranged) and 200 (server ignored Range) carry the
                // first bytes starting at offset 0, so either is usable here.
                return rc == CURLE_OK && (code == 206 || code == 200);
            },
            "size probe");
    } catch (const slideio::RuntimeError& e) {
        const std::string snip = responseBodySnippet(body);
        if (!snip.empty()) {
            RAISE_RUNTIME_ERROR << e.what() << "; server response: " << snip;
        }
        throw;
    }

    const uint64_t sz = probe.total ? probe.total : probe.length;
    if (sz == 0) return false;
    m_size = sz;

    // Prime block 0 with the bytes we just downloaded so the first read of the
    // TIFF header / first IFD is served without another round-trip. A 200 from a
    // non-conforming server delivers the whole file; keep only the first block.
    if (s_cacheEnabled.load() && !body.empty()) {
        if (body.size() > kBlockSize) body.resize(kBlockSize);
        m_cache.insert(0, std::move(body));
    }
    return true;
}

uint64_t HttpStream::size() const { return m_size; }
std::string HttpStream::uri() const { return m_url; }

std::vector<uint8_t> HttpStream::fetchBlocks(uint64_t firstBlock, uint64_t lastBlock)
{
    const uint64_t startByte = firstBlock * kBlockSize;
    // Clamp to EOF: the requested byte range must never extend past the object
    // size. The S3-style test fixture returns 416 for any range whose end is
    // >= size, so over-requesting would fail.
    const uint64_t endByte = std::min((lastBlock + 1) * kBlockSize, m_size) - 1;
    const std::string range = std::to_string(startByte) + "-" + std::to_string(endByte);

    std::vector<uint8_t> body;
    body.reserve(static_cast<size_t>(endByte - startByte + 1));

    // A 206 (Partial Content) is always correct: the body covers exactly the
    // requested range. A 200 (full body) is only safe when the run starts at
    // byte 0 -- then storeRun slices the leading blocks correctly. If a server
    // ignores the Range header and returns 200 for a run that starts past 0, the
    // body is file[0..] but storeRun would treat it as file[startByte..], silently
    // mis-slicing. Reject that case rather than serve wrong data.
    auto acceptRanged = [&](CURLcode rc, long code) {
        if (rc != CURLE_OK) return false;
        if (code == 206) return true;
        if (code == 200) {
            if (startByte == 0) return true;
            RAISE_RUNTIME_ERROR << "HttpStream: server ignored Range header; got 200"
                                   " for ranged request at offset " << startByte
                                << " for " << m_url;
        }
        return false;  // non-2xx -> retry/error path in performWithRetry
    };

    try {
        performWithRetry(
            static_cast<CURL*>(m_curl),
            [&](CURL* curl) {
                // body is appended to across attempts; reset so a failed partial
                // attempt does not corrupt the next one's data.
                body.clear();
                curl_easy_setopt(curl, CURLOPT_URL, m_url.c_str());
                curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str());
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, bodyCb);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
                curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
            },
            acceptRanged, "fetch");
    } catch (const slideio::RuntimeError& e) {
        // On failure `body` holds the last attempt's response (the S3 error
        // XML, for an AWS 4xx). Surface it so the user knows whether it's a
        // signature mismatch, an expired URL, an access-denied policy, etc.
        const std::string snip = responseBodySnippet(body);
        if (!snip.empty()) {
            RAISE_RUNTIME_ERROR << e.what() << "; server response: " << snip;
        }
        throw;
    }
    return body;
}

size_t HttpStream::read(uint64_t offset, size_t count, void* buf)
{
    if (count == 0 || offset >= m_size) return 0;
    if (offset + count > m_size) count = static_cast<size_t>(m_size - offset);
    if (count == 0) return 0;

    const uint64_t firstBlock = offset / kBlockSize;
    const uint64_t lastBlock  = (offset + count - 1) / kBlockSize;
    const bool useCache = s_cacheEnabled.load();

    std::lock_guard<std::mutex> lk(m_mutex);  // serialize cache + fetch + copy-out

    // Local staging for the cache-disabled path: maps block index -> bytes.
    // When the cache is enabled this stays empty and we serve from m_cache.
    std::map<uint64_t, std::vector<uint8_t>> staged;

    // Splits a fetched [runStart, runEnd] blob into per-block chunks and stores
    // them either into the member cache (enabled) or the local staging map.
    auto storeRun = [&](uint64_t runStart, uint64_t runEnd,
                        const std::vector<uint8_t>& blob) {
        for (uint64_t i = runStart; i <= runEnd; ++i) {
            const size_t blkOff = static_cast<size_t>((i - runStart) * kBlockSize);
            const size_t blkSize =
                std::min<size_t>(kBlockSize, blob.size() - blkOff);
            std::vector<uint8_t> block(blob.begin() + blkOff,
                                       blob.begin() + blkOff + blkSize);
            if (useCache) {
                m_cache.insert(i, std::move(block));
            } else {
                staged.emplace(i, std::move(block));
            }
        }
    };

    // Walk the requested block span. Cached blocks are skipped; consecutive
    // missing blocks are coalesced into a single ranged GET.
    uint64_t b = firstBlock;
    while (b <= lastBlock) {
        if (useCache && m_cache.contains(b)) { ++b; continue; }
        const uint64_t runStart = b;
        while (b <= lastBlock && !(useCache && m_cache.contains(b))) ++b;
        const uint64_t runEnd = b - 1;
        storeRun(runStart, runEnd, fetchBlocks(runStart, runEnd));
    }

    // Unified copy-out: pull each block (from cache or staging) and copy the
    // requested slice into the output buffer.
    auto getBlock = [&](uint64_t index, std::vector<uint8_t>& out) -> bool {
        if (useCache) return m_cache.get(index, out);
        auto it = staged.find(index);
        if (it == staged.end()) return false;
        out = it->second;
        return true;
    };

    uint8_t* dst = static_cast<uint8_t*>(buf);
    size_t written = 0;
    for (uint64_t i = firstBlock; i <= lastBlock; ++i) {
        std::vector<uint8_t> block;
        if (!getBlock(i, block)) {
            RAISE_RUNTIME_ERROR << "HttpStream: missing block " << i
                                << " for " << m_url;
        }
        // Keep the block's start byte in 64-bit arithmetic; narrowing the product
        // i * kBlockSize to size_t would truncate for blocks past 4 GB on 32-bit
        // builds. The first-block partial offset is < kBlockSize, so it narrows
        // safely.
        const uint64_t blockStartByte = i * static_cast<uint64_t>(kBlockSize);
        const size_t copyFrom =
            (i == firstBlock) ? static_cast<size_t>(offset - blockStartByte) : 0;
        const size_t remaining = count - written;
        const size_t copyN = std::min<size_t>(block.size() - copyFrom, remaining);
        std::memcpy(dst + written, block.data() + copyFrom, copyN);
        written += copyN;
    }
    return written;
}

void HttpStream::prefetch(uint64_t, size_t) {}

void HttpStream::setCacheEnabled(bool enabled) { s_cacheEnabled.store(enabled); }
bool HttpStream::cacheEnabled() { return s_cacheEnabled.load(); }

} // namespace slideio
