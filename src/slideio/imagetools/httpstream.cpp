// src/slideio/imagetools/httpstream.cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/imagetools/httpstream.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/base/log.hpp"

#include <curl/curl.h>
#include <cstring>

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
size_t discardCb(char*, size_t s, size_t n, void*) { return s * n; }
} // namespace

HttpStream::HttpStream(const std::string& url)
    : m_url(url), m_cache(kCacheCapacityBlocks)
{
    if (!probeSize()) {
        RAISE_RUNTIME_ERROR << "HttpStream: could not determine size of " << url;
    }
    SLIDEIO_LOG(INFO) << "HttpStream opened " << url << " size=" << m_size;
}

HttpStream::~HttpStream() = default;

bool HttpStream::probeSize()
{
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    uint64_t sz = 0;
    curl_easy_setopt(curl, CURLOPT_URL, m_url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &sz);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discardCb);
    CURLcode rc = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);
    if (rc != CURLE_OK || code >= 400 || sz == 0) return false;
    m_size = sz;
    return true;
}

uint64_t HttpStream::size() const { return m_size; }
std::string HttpStream::uri() const { return m_url; }

size_t HttpStream::read(uint64_t /*offset*/, size_t /*count*/, void* /*buf*/)
{
    RAISE_RUNTIME_ERROR << "HttpStream::read not implemented yet";
}

void HttpStream::prefetch(uint64_t, size_t) {}

void HttpStream::setCacheEnabled(bool enabled) { s_cacheEnabled.store(enabled); }
bool HttpStream::cacheEnabled() { return s_cacheEnabled.load(); }

std::vector<uint8_t> HttpStream::fetchBlocks(uint64_t, uint64_t)
{
    return {};  // placeholder, implemented in E4
}

} // namespace slideio
