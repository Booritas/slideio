// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/base/logcontract.hpp"

#include <list>
#include <sstream>
#include <string>
#include <vector>

namespace slideio
{
    // Two call sites (czislide.cpp, otscene.cpp) stream a container of names
    // directly, relying on the container operator<< that glog's
    // glog/stl_logging.h used to inject globally. That header is gone along
    // with the rest of glog, and call sites are frozen by contract (see
    // SLIDEIO_LOG below), so the two shapes actually used are reproduced here
    // instead. Declared before LogStream so ordinary unqualified lookup from
    // slideio::detail finds them - the same path slideio's other operator<<
    // overloads (e.g. TiffDirectory's) already rely on.
    inline std::ostream& operator<<(std::ostream& stream, const std::vector<std::string>& values)
    {
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i != 0) {
                stream << ", ";
            }
            stream << values[i];
        }
        return stream;
    }

    inline std::ostream& operator<<(std::ostream& stream, const std::list<std::string>& values)
    {
        bool first = true;
        for (const auto& value : values) {
            if (!first) {
                stream << ", ";
            }
            stream << value;
            first = false;
        }
        return stream;
    }

    namespace detail
    {
        // Pasteable severity constants. The severity is embedded in the
        // identifier so SLIDEIO_LOG can token-paste it and never expand a bare
        // INFO / WARNING / ERROR token - <wingdi.h> defines ERROR as 0. This is
        // the same technique glog used: LOG(severity) pasted
        // COMPACT_GOOGLE_LOG_##severity. See design spec section 4.5.1.
        inline constexpr int severityINFO = static_cast<int>(LogLevel::Info);
        inline constexpr int severityWARNING = static_cast<int>(LogLevel::Warning);
        inline constexpr int severityERROR = static_cast<int>(LogLevel::Error);
        inline constexpr int severityFATAL = static_cast<int>(LogLevel::Fatal);

        /** @brief Cheap per-call-site threshold check.
         *
         * The pointer is cached once per module. Duplicating the cached pointer
         * across modules is intentional and correct: every copy points at the
         * one variable inside slideio-base. See design spec section 4.3.
         */
        inline bool logEnabled(int level)
        {
            static const int* threshold = logThresholdPtr();
            return level >= *threshold;
        }

        /** @brief Accumulates a streamed message and emits it on destruction.
         *
         * Header-local by design: the std::ostringstream never crosses a module
         * boundary, so there is no C4251 and no CRT coupling. Only the
         * const char* handed to logMessage crosses.
         */
        class LogStream
        {
        public:
            LogStream(int level, const char* file, int line)
                : m_level(level), m_file(file), m_line(line) {}

            ~LogStream()
            {
                logMessage(m_level, m_file, m_line, m_stream.str().c_str());
            }

            LogStream(const LogStream&) = delete;
            LogStream& operator=(const LogStream&) = delete;

            template <typename T>
            LogStream& operator<<(const T& value)
            {
                m_stream << value;
                return *this;
            }

            // Non-template overload for stream manipulators (std::endl is the
            // only one any of the 234 call sites use). A manipulator is itself
            // an overloaded/template function, so the generic operator<<(const
            // T&) above cannot deduce T from it - the parameter type here is
            // concrete, so overload resolution can pick the right std::endl
            // specialization the way it would for a real std::ostream. Call
            // sites are frozen by contract, so this is handled here rather
            // than by touching any of them.
            LogStream& operator<<(std::ostream& (*manip)(std::ostream&))
            {
                manip(m_stream);
                return *this;
            }

        private:
            std::ostringstream m_stream;
            int m_level;
            const char* m_file;
            int m_line;
        };
    }
}

// The switch(0) case 0: default: prefix makes the macro immune to dangling
// else. No current call site needs it; it costs nothing to be safe.
#define SLIDEIO_LOG(LEVEL)                                                     \
    switch (0) case 0: default:                                                \
    if (!slideio::detail::logEnabled(slideio::detail::severity##LEVEL)) ; else  \
        slideio::detail::LogStream(slideio::detail::severity##LEVEL,            \
                                   __FILE__, __LINE__)
