// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// This is the ONLY translation unit in slideio that includes a third-party
// logging library. See the design spec, section 4.3: everything else reaches
// logging through the three exported functions declared in logcontract.hpp.
#include "slideio/base/logcontract.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>

#include <cstring>
#include <memory>

#include <chrono>
#include <cstdio>
#include <ctime>
#include <sstream>
#include <string>
#include <thread>

namespace
{
    // Constant-initialised, NOT assigned in a lazy init function. A log emitted
    // during another translation unit's static initialisation must not read an
    // indeterminate threshold. See spec section 4.7.1.
    int g_threshold = static_cast<int>(slideio::LogLevel::Fatal);

    // Reproduces glog's severity initial: I / W / E / F.
    char severityInitial(int level)
    {
        switch (level) {
        case static_cast<int>(slideio::LogLevel::Info):    return 'I';
        case static_cast<int>(slideio::LogLevel::Warning): return 'W';
        case static_cast<int>(slideio::LogLevel::Error):   return 'E';
        default:                                           return 'F';
        }
    }

    // glog emits the basename only. Handles both separators: __FILE__ is
    // backslash-separated under MSVC and slash-separated elsewhere.
    const char* basename(const char* path)
    {
        if (path == nullptr) {
            return "";
        }
        const char* last = path;
        for (const char* p = path; *p != '\0'; ++p) {
            if (*p == '/' || *p == '\\') {
                last = p + 1;
            }
        }
        return last;
    }

    // glog's timestamp: yyyymmdd hh:mm:ss.uuuuuu, local time, microseconds.
    // Formatted explicitly rather than through fmt's chrono support, which
    // would add an <spdlog/fmt/chrono.h> dependency and leave the fractional
    // width up to the clock's precision.
    std::string timestamp()
    {
        const auto now = std::chrono::system_clock::now();
        const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
        const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
                                now.time_since_epoch()).count() % 1000000;

        std::tm parts{};
#if defined(_MSC_VER)
        localtime_s(&parts, &seconds);
#else
        localtime_r(&seconds, &parts);
#endif

        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%04d%02d%02d %02d:%02d:%02d.%06lld",
                      parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday,
                      parts.tm_hour, parts.tm_min, parts.tm_sec,
                      static_cast<long long>(micros));
        return buffer;
    }

    // glog prints the OS thread id. std::thread::id streams as a number on
    // every target toolchain; the exact value need not match glog's, only the
    // field's presence and shape.
    std::string threadId()
    {
        std::ostringstream stream;
        stream << std::this_thread::get_id();
        return stream.str();
    }

    // Builds a fresh sink/logger for every call rather than caching one
    // process-wide.
    //
    // A cached singleton was tried first (via spdlog::stderr_logger_mt(),
    // stored in a function-local static) and failed under test: spdlog's
    // Windows stdout/stderr sink resolves the raw OS HANDLE for its target
    // FILE* exactly once, at construction
    // (stdout_sink_base ctor: handle_ = ::_get_osfhandle(::_fileno(file_))),
    // and never re-resolves it on later writes. A singleton therefore keeps
    // writing to whatever OS handle sat behind stderr's file descriptor at
    // the moment of the *first* log call in the process, even after stderr
    // is later reopened or redirected - which is exactly what
    // testing::internal::CaptureStderr() does before every test. The
    // resulting write targets a stale handle, spdlog's own logger::sink_it_
    // catches the resulting exception internally and hands it to the error
    // handler (see below) before it ever reaches this file's try/catch, so
    // the message is silently lost with no visible failure. Building the
    // sink fresh on every call re-resolves the handle each time and keeps it
    // always current. The extra allocation is a non-issue: logMessage sits
    // on the error/exception path (spec 4.4.5: ~97% of ERROR volume comes
    // from RuntimeError), not a hot loop.
    //
    // Constructing the logger directly (rather than through
    // spdlog::stderr_logger_mt(), which registers it by name in spdlog's
    // global registry) also sidesteps "logger already exists" on the second
    // call, and means the per-logger error handler - not the process-wide
    // spdlog::set_error_handler() - is what needs to be set here.
    void logLine(const std::string& line)
    {
        auto sink = std::make_shared<spdlog::sinks::stderr_sink_mt>();
        spdlog::logger logger("slideio", sink);
        // Non-throwing error handler: see spec 4.5.2. Belt to the try/catch
        // in logMessage.
        logger.set_error_handler([](const std::string&) {});
        // The whole line is produced by logMessage; spdlog must not add a
        // prefix of its own.
        logger.set_pattern("%v");
        logger.set_level(spdlog::level::trace);
        logger.flush_on(spdlog::level::trace);
        logger.info(line);
    }
}

namespace slideio
{
    void setLogThreshold(int level) noexcept
    {
        if (level < static_cast<int>(LogLevel::Info) || level > static_cast<int>(LogLevel::Fatal)) {
            return;
        }
        g_threshold = level;
    }

    const int* logThresholdPtr() noexcept
    {
        return &g_threshold;
    }

    void logMessage(int level, const char* file, int line, const char* message) noexcept
    {
        if (level < g_threshold) {
            return;
        }
        try {
            // E20260822 19:31:43.120792 18220 exceptions.cpp:10] message
            std::ostringstream line_;
            line_ << severityInitial(level) << timestamp() << ' ' << threadId() << ' '
                  << basename(file) << ':' << line << "] "
                  << (message == nullptr ? "" : message);
            logLine(line_.str());
        }
        catch (...) {
            // Swallowed deliberately. This runs during throw; propagating here
            // is std::terminate. See spec 4.5.2.
        }
    }
}
