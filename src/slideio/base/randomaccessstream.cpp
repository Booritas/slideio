// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/base/randomaccessstream.hpp"

namespace slideio
{
    // Out-of-line definitions anchor the vtable in slideio-base.dll so the
    // exported (SLIDEIO_BASE_EXPORTS) symbols are emitted exactly once.
    RandomAccessStream::~RandomAccessStream() = default;

    void RandomAccessStream::prefetch(uint64_t /*offset*/, size_t /*count*/) {}
}
