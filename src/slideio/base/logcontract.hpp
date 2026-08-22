// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/base/slideio_base_def.hpp"

namespace slideio
{
    /** @brief Log severity.
     *
     * Enumerator names deliberately avoid the bare tokens INFO, WARNING, ERROR
     * and FATAL: <wingdi.h> defines ERROR as 0, which would turn the
     * declaration below into "0 = 2". See the design spec, section 4.5.1.
     *
     * Fatal is a threshold-only value. Nothing in slideio logs at Fatal, and
     * unlike glog's LOG(FATAL) this library never aborts the process.
     */
    enum class LogLevel
    {
        Info = 0,
        Warning = 1,
        Error = 2,
        Fatal = 3
    };

    /** @brief Emits one formatted line to stderr if @a level passes the threshold.
     *
     * noexcept is a hard requirement, not decoration: this is called from
     * RuntimeError's copy constructor while an exception is being thrown, where
     * an escaping exception means std::terminate. All sink errors are swallowed
     * - a dropped log line always beats terminating the caller.
     *
     * @a file may be a full path; only its basename is emitted. @a file and
     * @a message tolerate nullptr.
     */
    SLIDEIO_BASE_EXPORTS void logMessage(int level, const char* file, int line,
                                         const char* message) noexcept;

    /** @brief Sets the global severity threshold. Values outside LogLevel are ignored. */
    SLIDEIO_BASE_EXPORTS void setLogThreshold(int level) noexcept;

    /** @brief Address of the single global threshold, for cheap per-call-site checks.
     *
     * The returned pointer is stable for the process lifetime and always refers
     * to the one variable inside slideio-base. Callers in other modules cache it
     * so the common case is a load rather than a cross-DLL call.
     */
    SLIDEIO_BASE_EXPORTS const int* logThresholdPtr() noexcept;
}
