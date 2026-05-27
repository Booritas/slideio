// src/tests/main/http_fixture/http_fixture.hpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include <filesystem>
#include <string>

namespace slideio { namespace tests {

// Launches the Python test HTTP server (test_http_server.py) as a child process
// rooted at a given directory on an ephemeral port. Reads the printed PORT=<n>
// line to learn the bound port. Terminates the child on destruction.
class HttpFixture {
public:
    explicit HttpFixture(const std::filesystem::path& rootDir);
    ~HttpFixture();

    int port() const { return m_port; }
    std::string url(const std::string& path) const;

    // Control channel (libcurl one-shot calls against the server).
    // Makes the server fail the next `count` file GETs with HTTP 503.
    void failNextGets(int count);
    // Number of successful file GETs the server has served so far.
    int servedCount() const;

    HttpFixture(const HttpFixture&) = delete;
    HttpFixture& operator=(const HttpFixture&) = delete;

private:
    int m_port = 0;
#ifdef _WIN32
    void* m_processHandle = nullptr;  // HANDLE
    void* m_threadHandle = nullptr;   // HANDLE
#else
    int m_pid = -1;
#endif
};

}} // namespace slideio::tests
