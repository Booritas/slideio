// src/tests/main/http_fixture/http_fixture.cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "http_fixture.hpp"

#include <curl/curl.h>

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <signal.h>
  #include <unistd.h>
  #include <sys/wait.h>
#endif

#ifndef SLIDEIO_TEST_PYTHON
  #error "SLIDEIO_TEST_PYTHON not defined (set by CMake)"
#endif
#ifndef SLIDEIO_TEST_HTTP_SERVER
  #error "SLIDEIO_TEST_HTTP_SERVER not defined (set by CMake)"
#endif

namespace slideio { namespace tests {

namespace {
// Parses the integer following "PORT=" in the buffered child stdout.
int parsePortLine(const std::string& text) {
    auto eq = text.find("PORT=");
    if (eq == std::string::npos) {
        throw std::runtime_error("HttpFixture: fixture did not print PORT=");
    }
    return std::stoi(text.substr(eq + 5));
}

size_t collectBodyCb(char* data, size_t size, size_t nmemb, void* ud) {
    auto* out = static_cast<std::string*>(ud);
    out->append(data, size * nmemb);
    return size * nmemb;
}
} // namespace

#ifdef _WIN32

HttpFixture::HttpFixture(const std::filesystem::path& rootDir)
{
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!::CreatePipe(&readPipe, &writePipe, &sa, 0)) {
        throw std::runtime_error("HttpFixture: CreatePipe failed");
    }
    // The read end must NOT be inherited by the child.
    if (!::SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0)) {
        ::CloseHandle(readPipe);
        ::CloseHandle(writePipe);
        throw std::runtime_error("HttpFixture: SetHandleInformation failed");
    }

    std::string cmd;
    cmd += "\"";
    cmd += SLIDEIO_TEST_PYTHON;
    cmd += "\" \"";
    cmd += SLIDEIO_TEST_HTTP_SERVER;
    cmd += "\" --root \"";
    cmd += rootDir.string();
    cmd += "\" --port 0";

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;
    si.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};
    std::string mutableCmd = cmd;  // CreateProcessA may modify the buffer.
    BOOL ok = ::CreateProcessA(
        nullptr, mutableCmd.data(), nullptr, nullptr,
        TRUE, /*bInheritHandles*/
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (!ok) {
        DWORD err = ::GetLastError();
        ::CloseHandle(readPipe);
        ::CloseHandle(writePipe);
        throw std::runtime_error("HttpFixture: CreateProcess failed err="
                                 + std::to_string(err));
    }
    // Close the write end in the parent so reads unblock once the child writes
    // (and so we are not the sole keeper of the pipe).
    ::CloseHandle(writePipe);

    // Read until we have a full PORT=<n>\n line. Don't read to EOF -- the
    // server runs forever and would keep the pipe open.
    std::string buf;
    char chunk[256];
    bool gotLine = false;
    while (!gotLine) {
        DWORD got = 0;
        BOOL rok = ::ReadFile(readPipe, chunk, sizeof(chunk), &got, nullptr);
        if (!rok || got == 0) break;  // pipe closed / child died
        buf.append(chunk, got);
        if (buf.find('\n') != std::string::npos) gotLine = true;
    }
    ::CloseHandle(readPipe);

    if (!gotLine) {
        ::TerminateProcess(pi.hProcess, 0);
        ::WaitForSingleObject(pi.hProcess, 2000);
        ::CloseHandle(pi.hProcess);
        ::CloseHandle(pi.hThread);
        throw std::runtime_error("HttpFixture: could not read fixture port");
    }

    m_port = parsePortLine(buf);
    m_processHandle = pi.hProcess;
    m_threadHandle = pi.hThread;
}

HttpFixture::~HttpFixture()
{
    if (m_processHandle) {
        ::TerminateProcess(static_cast<HANDLE>(m_processHandle), 0);
        ::WaitForSingleObject(static_cast<HANDLE>(m_processHandle), 2000);
        ::CloseHandle(static_cast<HANDLE>(m_processHandle));
    }
    if (m_threadHandle) {
        ::CloseHandle(static_cast<HANDLE>(m_threadHandle));
    }
}

#else // POSIX

HttpFixture::HttpFixture(const std::filesystem::path& rootDir)
{
    int pipefd[2];
    if (pipe(pipefd) < 0) throw std::runtime_error("HttpFixture: pipe failed");
    pid_t pid = fork();
    if (pid < 0) throw std::runtime_error("HttpFixture: fork failed");
    if (pid == 0) {
        // child: redirect stdout to the pipe, exec python.
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        execlp(SLIDEIO_TEST_PYTHON, SLIDEIO_TEST_PYTHON,
               SLIDEIO_TEST_HTTP_SERVER,
               "--root", rootDir.string().c_str(),
               "--port", "0",
               (char*)nullptr);
        _exit(127);
    }
    close(pipefd[1]);
    std::string buf;
    char chunk[256];
    bool gotLine = false;
    while (!gotLine) {
        ssize_t n = ::read(pipefd[0], chunk, sizeof(chunk));
        if (n <= 0) break;
        buf.append(chunk, static_cast<size_t>(n));
        if (buf.find('\n') != std::string::npos) gotLine = true;
    }
    close(pipefd[0]);
    if (!gotLine) {
        kill(pid, SIGTERM);
        int st;
        waitpid(pid, &st, 0);
        throw std::runtime_error("HttpFixture: could not read fixture port");
    }
    m_port = parsePortLine(buf);
    m_pid = pid;
}

HttpFixture::~HttpFixture()
{
    if (m_pid > 0) {
        kill(m_pid, SIGTERM);
        int st;
        waitpid(m_pid, &st, 0);
    }
}

#endif

std::string HttpFixture::url(const std::string& path) const
{
    return "http://127.0.0.1:" + std::to_string(m_port) + "/" + path;
}

void HttpFixture::failNextGets(int count)
{
    std::string u = "http://127.0.0.1:" + std::to_string(m_port)
                  + "/__control__/fail-next/" + std::to_string(count);
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("HttpFixture::failNextGets: curl_easy_init failed");
    curl_easy_setopt(curl, CURLOPT_URL, u.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
    CURLcode rc = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);
    if (rc != CURLE_OK || code != 204) {
        throw std::runtime_error("HttpFixture::failNextGets: control POST failed code="
                                 + std::to_string(code));
    }
}

int HttpFixture::servedCount() const
{
    std::string u = "http://127.0.0.1:" + std::to_string(m_port) + "/__control__/stats";
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("HttpFixture::servedCount: curl_easy_init failed");
    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL, u.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collectBodyCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    CURLcode rc = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);
    if (rc != CURLE_OK || code != 200) {
        throw std::runtime_error("HttpFixture::servedCount: control GET failed code="
                                 + std::to_string(code));
    }
    auto pos = body.find("served=");
    if (pos == std::string::npos) {
        throw std::runtime_error("HttpFixture::servedCount: malformed stats body");
    }
    return std::stoi(body.substr(pos + 7));
}

}} // namespace slideio::tests
