// src/slideio/imagetools/uridispatcher.cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/imagetools/uridispatcher.hpp"
#include "slideio/imagetools/filestream.hpp"
#include "slideio/imagetools/httpstream.hpp"
#include "slideio/base/exceptions.hpp"

#include <cctype>
#include <cstring>

namespace slideio {

static bool ciStartsWith(const std::string& s, const char* p) {
    size_t n = std::strlen(p);
    if (s.size() < n) return false;
    for (size_t i = 0; i < n; ++i)
        if (std::tolower(static_cast<unsigned char>(s[i])) !=
            std::tolower(static_cast<unsigned char>(p[i]))) return false;
    return true;
}

UriScheme detectUriScheme(const std::string& uri) {
    if (ciStartsWith(uri, "s3://"))   return UriScheme::S3;
    if (ciStartsWith(uri, "http://")) return UriScheme::Http;
    if (ciStartsWith(uri, "https://"))return UriScheme::Http;
    // file:// and everything else are local
    return UriScheme::LocalFile;
}

std::string s3UriToHttps(const std::string& uri) {
    if (!ciStartsWith(uri, "s3://")) return uri;
    auto rest = uri.substr(5);
    auto slash = rest.find('/');
    if (slash == std::string::npos)
        RAISE_RUNTIME_ERROR << "s3 URI missing key: " << uri;
    std::string bucket = rest.substr(0, slash);
    std::string key = rest.substr(slash + 1);
    return "https://" + bucket + ".s3.amazonaws.com/" + key;
}

static std::string stripFileScheme(const std::string& uri) {
    if (ciStartsWith(uri, "file://")) return uri.substr(7);
    return uri;
}

std::shared_ptr<RandomAccessStream> createStream(const std::string& uri) {
    switch (detectUriScheme(uri)) {
        case UriScheme::LocalFile: return std::make_shared<FileStream>(stripFileScheme(uri));
        case UriScheme::S3:        return std::make_shared<HttpStream>(s3UriToHttps(uri));
        case UriScheme::Http:      return std::make_shared<HttpStream>(uri);
    }
    RAISE_RUNTIME_ERROR << "createStream: unknown URI scheme: " << uri;
}

std::string uriResourceName(const std::string& uri) {
    std::string u = uri;
    // strip query string
    auto q = u.find('?');
    if (q != std::string::npos) u.erase(q);
    // strip schemes
    if (ciStartsWith(u, "file://"))  u.erase(0, 7);
    else if (ciStartsWith(u, "s3://"))    u.erase(0, 5);
    else if (ciStartsWith(u, "http://"))  u.erase(0, 7);
    else if (ciStartsWith(u, "https://")) u.erase(0, 8);
    // return last path segment
    auto slash = u.find_last_of("/\\");
    return (slash == std::string::npos) ? u : u.substr(slash + 1);
}

std::string siblingUri(const std::string& base, const std::string& name) {
    // If `name` already has a scheme, use it as-is.
    if (ciStartsWith(name, "s3://") || ciStartsWith(name, "http://")
        || ciStartsWith(name, "https://") || ciStartsWith(name, "file://")) {
        return name;
    }
    // Find the position after the last '/' or '\\' in `base` (after stripping query).
    std::string b = base;
    auto q = b.find('?');
    if (q != std::string::npos) b.erase(q);
    auto slash = b.find_last_of("/\\");
    if (slash == std::string::npos) return name;
    return b.substr(0, slash + 1) + name;
}

} // namespace slideio
