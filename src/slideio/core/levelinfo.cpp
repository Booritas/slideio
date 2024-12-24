// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/levelinfo.hpp"
#include <sstream>

using namespace slideio;

std::string LevelInfo::toString() const {
    std::stringstream ss;
    ss << *this;
    return ss.str();
}
