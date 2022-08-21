// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#pragma once
#include "slideio/slideio/slideio_def.hpp"
#include "slideio/slideio/slide.hpp"
#include <string>
#include <vector>

namespace  slideio
{
    SLIDEIO_EXPORTS std::shared_ptr<Slide> openSlide(const std::string& path, const std::string& driver);
    SLIDEIO_EXPORTS std::vector<std::string> getDriverIDs();
}