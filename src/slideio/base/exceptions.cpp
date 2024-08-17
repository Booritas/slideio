// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/base/exceptions.hpp"
#include "slideio/base/log.hpp"

using namespace slideio;

void RuntimeError::log(const std::string& message) {
    SLIDEIO_LOG(ERROR) << message;
}
