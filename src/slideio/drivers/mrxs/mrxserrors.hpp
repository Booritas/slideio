// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/core/exceptions.hpp"

// Every MRXS failure is a slideio::RuntimeError whose message starts with one of three
// fixed tags (design section 8), so callers and tests can tell the categories apart:
//   MRXS corrupt:      the bundle contradicts the format or itself
//   MRXS unsupported:  a valid bundle outside the profile this driver reads
//   MRXS limit:        a resource budget was exceeded
// They are thrown directly rather than through RAISE_RUNTIME_ERROR, which would put
// the source file and line in front of the tag.
#define MRXS_CORRUPT throw slideio::RuntimeError() << "MRXS corrupt: "
#define MRXS_UNSUPPORTED throw slideio::RuntimeError() << "MRXS unsupported: "
#define MRXS_LIMIT throw slideio::RuntimeError() << "MRXS limit: "
