// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// This is the ONLY translation unit in slideio that includes a third-party
// logging library. See the design spec, section 4.3: everything else reaches
// logging through the three exported functions declared in logcontract.hpp.
#include "slideio/core/logcontract.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <sstream>
#include <string>

// For the spec 4.6 thread id field, which must be the OS thread id in decimal.
#if defined(WIN32)
// Narrow the surface and keep the min/max macros out: this TU is the one place
// that mixes a third-party logging library with a platform header, and the
// LogMacroCollision tests exist because that mixture has bitten before.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <pthread.h>
#else
#include <sys/syscall.h>
#include <unistd.h>
#endif

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
        // `seconds` and `micros` are both derived from ONE microsecond count
        // rather than from independent calls (system_clock::to_time_t(now)
        // for the former, a separate floor-division of
        // now.time_since_epoch() for the latter): to_time_t's rounding
        // direction is implementation-defined, so the two could otherwise
        // disagree by a whole second once per second (e.g. to_time_t rounds
        // up to the next second while micros still reflects the previous
        // one). A single division can't disagree with itself.
        const auto now = std::chrono::system_clock::now();
        const auto usSinceEpoch = std::chrono::duration_cast<std::chrono::microseconds>(
                                       now.time_since_epoch()).count();
        const std::time_t seconds = static_cast<std::time_t>(usSinceEpoch / 1000000);
        const auto micros = usSinceEpoch % 1000000;

        // Zero-initialised, so a localtime_s/r failure - only possible for a
        // time_t outside the platform's representable range, which "now"
        // never is - degrades to a fixed, parseable (if meaningless)
        // timestamp rather than reading uninitialised memory. The return
        // value is deliberately not checked beyond that guarantee.
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

    // glog prints the OS thread id as a decimal integer, and spec 4.6 pins that
    // shape. Streaming std::this_thread::get_id() is not a substitute: the
    // formatting of std::thread::id is implementation defined, and libc++ prints
    // the underlying pthread_t as a hex pointer ("0x1f2755d80"), which breaks the
    // documented field. Ask the OS for its numeric thread id instead.
    std::string threadId()
    {
#if defined(WIN32)
        return std::to_string(static_cast<unsigned long long>(::GetCurrentThreadId()));
#elif defined(__APPLE__)
        std::uint64_t id = 0;
        ::pthread_threadid_np(nullptr, &id);
        return std::to_string(id);
#else
        return std::to_string(static_cast<unsigned long long>(::syscall(SYS_gettid)));
#endif
    }

    // Builds a fresh sink for every call rather than caching one
    // process-wide, and calls the sink directly - no spdlog::logger.
    //
    // Freshness: a cached singleton was tried first (via
    // spdlog::stderr_logger_mt(), stored in a function-local static) and
    // failed under test: spdlog's Windows stdout/stderr sink resolves the
    // raw OS HANDLE for its target FILE* exactly once, at construction
    // (stdout_sink_base ctor: handle_ = ::_get_osfhandle(::_fileno(file_))),
    // and never re-resolves it on later writes. A singleton therefore keeps
    // writing to whatever OS handle sat behind stderr's file descriptor at
    // the moment of the *first* log call in the process, even after stderr
    // is later reopened or redirected - which is exactly what
    // testing::internal::CaptureStderr() does before every test. Building
    // the sink fresh on every call re-resolves the handle each time and
    // keeps it always current. The extra allocation is a non-issue: the
    // caller (logMessage) has already returned before this point unless a
    // line is actually going to be emitted - the threshold check happens
    // first and costs nothing when nothing is logged, which is the common
    // case (default threshold is Fatal, and even at INFO most call sites
    // never fire). Once a line IS emitted, a synchronous flushed write
    // dominates whatever this allocates.
    //
    // No spdlog::logger: constructing one via spdlog::stderr_logger_mt()
    // registers it by name in spdlog's global registry, which throws
    // "logger already exists" on the second call now that construction
    // happens per-call rather than once. Calling the sink's log() directly
    // sidesteps the registry entirely, and also avoids building two
    // pattern_formatters per call - stdout_sink_base's constructor
    // unconditionally builds one for its default pattern, and going through
    // a logger would mean set_pattern("%v") immediately discarding it and
    // building a second - plus a logger's flush_on(), which would be a
    // redundant second console-mutex acquisition and fflush: the sink's own
    // log() already flushes after every write (see stdout_sinks-inl.h).
    void logLine(const std::string& line)
    {
        spdlog::sinks::stderr_sink_mt sink;
        // The whole line is produced by logMessage; spdlog must not add a
        // prefix of its own.
        sink.set_pattern("%v");
        sink.log(spdlog::details::log_msg(spdlog::source_loc{}, "slideio", spdlog::level::info, line));
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
            // This is the only guard against a sink failure escaping:
            // logLine() calls the sink's log() directly, with no
            // spdlog::logger in the path, so there is no SPDLOG_LOGGER_CATCH
            // to swallow the exception first (as it did in an earlier
            // version of this file that went through a logger - see git
            // history). Swallowed deliberately - this can run during throw,
            // where propagating is std::terminate. See spec 4.5.2.
        }
    }
}
