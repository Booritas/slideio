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

size_t headerCb(char* data, size_t size, size_t nmemb, void* ud) {
    auto* sz = static_cast<uint64_t*>(ud);
    std::string h(data, size * nmemb);
    const char* prefix = "Content-Length:";
    const size_t n = std::strlen(prefix);
    if (h.size() >= n && SLIDEIO_STRNCASECMP(h.c_str(), prefix, n) == 0) {
        *sz = std::strtoull(h.c_str() + n, nullptr, 10);
    }
    return size * nmemb;
}

// Parses the total object size from a "Content-Range: bytes 0-0/SIZE" header.
size_t headerCbContentRange(char* data, size_t size, size_t nmemb, void* ud) {
    auto* sz = static_cast<uint64_t*>(ud);
    std::string h(data, size * nmemb);
    const char* prefix = "Content-Range:";
    const size_t n = std::strlen(prefix);
    if (h.size() >= n && SLIDEIO_STRNCASECMP(h.c_str(), prefix, n) == 0) {
        auto slash = h.find('/');
        if (slash != std::string::npos) {
            *sz = std::strtoull(h.c_str() + slash + 1, nullptr, 10);
        }
    }
    return size * nmemb;
}

size_t discardCb(char*, size_t s, size_t n, void*) { return s * n; }

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
// (3 attempts; 50, 200, 800ms between attempts). Each attempt builds a fresh
// curl handle, hands it to `configure` (which sets URL/method/callbacks), and
// performs it. `accept` decides whether the (rc, httpCode) pair is a success;
// on success this returns normally. Transient failures are retried; a
// non-transient failure, or exhausting the budget, throws via RAISE_RUNTIME_ERROR
// with `context` in the message. Returns the last (rc, httpCode) on success.
struct RequestResult { CURLcode rc; long httpCode; };

RequestResult performWithRetry(const std::function<void(CURL*)>& configure,
                               const std::function<bool(CURLcode, long)>& accept,
                               const std::string& context) {
    constexpr int kMaxAttempts = 3;
    int delayMs = 50;
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        CURL* curl = curl_easy_init();
        if (!curl) RAISE_RUNTIME_ERROR << "HttpStream: curl_easy_init failed";
        configure(curl);
        CURLcode rc = curl_easy_perform(curl);
        long code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        curl_easy_cleanup(curl);

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
    if (!probeSize()) {
        RAISE_RUNTIME_ERROR << "HttpStream: could not determine size of " << url;
    }
    SLIDEIO_LOG(INFO) << "HttpStream opened " << url << " size=" << m_size;
}

HttpStream::~HttpStream() = default;

bool HttpStream::probeSize()
{
    // TODO(spec §8.4): verify Accept-Ranges: bytes at construction and fail fast
    // if the server does not advertise range support.

    // Both strategies share the retry policy in performWithRetry, so a transient
    // 5xx/connection blip at construction is retried just like a block fetch.
    // The `accept` predicate treats any non-error response (curl OK, code < 400)
    // as success: a 200 HEAD with no Content-Length is not a failure here -- we
    // simply fall through to the Content-Range strategy when sz stays 0.
    auto acceptNonError = [](CURLcode rc, long code) {
        return rc == CURLE_OK && code < 400;
    };

    // First attempt: HEAD with Content-Length.
    // HEAD may be rejected by some servers: AWS S3 presigned URLs are signed
    // for one HTTP method and return 403 SignatureDoesNotMatch when the method
    // doesn't match (the AWS CLI's `aws s3 presign` produces GET-signed URLs).
    // Other servers that don't implement HEAD return 405 or 501. Treat those
    // codes as "HEAD not available" and fall through to the Content-Range GET
    // probe instead of failing the whole open.
    {
        uint64_t sz = 0;
        const RequestResult headRes = performWithRetry(
            [&](CURL* curl) {
                curl_easy_setopt(curl, CURLOPT_URL, m_url.c_str());
                curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
                // On Windows, Conan's libcurl/OpenSSL builds ship without a default
                // CA bundle path; without NATIVE_CA the Windows certificate store is
                // ignored and every HTTPS peer fails verification.
                curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
                curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCb);
                curl_easy_setopt(curl, CURLOPT_HEADERDATA, &sz);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discardCb);
            },
            [](CURLcode rc, long code) {
                if (rc != CURLE_OK) return false;
                if (code < 400) return true;
                return code == 403 || code == 405 || code == 501;
            },
            "HEAD size probe");
        // Only trust the parsed Content-Length when HEAD itself succeeded.
        // On a 403/405/501 the body is an error message whose Content-Length
        // would otherwise be misread as the object size.
        if (headRes.httpCode < 400 && sz > 0) {
            m_size = sz;
            return true;
        }
        if (headRes.httpCode >= 400) {
            SLIDEIO_LOG(INFO) << "HttpStream: HEAD probe returned "
                              << headRes.httpCode << " for " << m_url
                              << "; falling back to GET Range size probe.";
        }
    }

    // Fallback: HEAD gave no Content-Length. Issue GET Range: bytes=0-0 and
    // parse the total size from the Content-Range response header.
    {
        uint64_t sz = 0;
        std::vector<uint8_t> body;
        try {
            performWithRetry(
                [&](CURL* curl) {
                    // body is appended to across attempts; reset so a failed
                    // attempt does not contaminate the next one's response.
                    body.clear();
                    curl_easy_setopt(curl, CURLOPT_URL, m_url.c_str());
                    curl_easy_setopt(curl, CURLOPT_RANGE, "0-0");
                    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
                    curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
                    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCbContentRange);
                    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &sz);
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, bodyCb);
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
                },
                acceptNonError, "Content-Range size probe");
        } catch (const slideio::RuntimeError& e) {
            const std::string snip = responseBodySnippet(body);
            if (!snip.empty()) {
                RAISE_RUNTIME_ERROR << e.what() << "; server response: " << snip;
            }
            throw;
        }
        if (sz > 0) {
            m_size = sz;
            return true;
        }
    }
    return false;
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
