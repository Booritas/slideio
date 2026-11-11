// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/lif/liftimestamps.hpp"

using namespace slideio;

std::vector<uint64_t> lif::parseTimestampTicks(const tinyxml2::XMLElement* /*image*/)
{
    // Filled in by the timestamps task.
    return {};
}
