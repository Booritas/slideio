// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/slideio_core_def.hpp"

namespace slideio
{
    /**
     * Per-thread mutable state for one block read.
     *
     * A driver derives from this and adds whatever its read path cannot share
     * between threads: a file handle, a scratch buffer, a parsed container
     * object. Instances are handed out one borrower at a time by ContextPool,
     * so a driver may treat the contents as exclusively its own for the
     * duration of a read and needs no further locking.
     *
     * This is where such state belongs -- not in thread-local storage, which
     * would tie a file handle's lifetime to a thread rather than to the Scene
     * that owns it.
     */
    class SLIDEIO_CORE_EXPORTS ReadContext
    {
    public:
        virtual ~ReadContext() = default;
        ReadContext(const ReadContext&) = delete;
        ReadContext& operator=(const ReadContext&) = delete;
    protected:
        ReadContext() = default;
    };
}
