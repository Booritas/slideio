// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/core/slideio_core_def.hpp"
#include <map>
#include <string>

namespace slideio
{
    /**@brief options a caller passes to openSlide, as key -> value strings.
     *
     * A key is `<driver-id>.<name>`: the prefix is compared case-insensitively with
     * ImageDriver::getID(), the name exactly. ImageDriverManager validates the prefixes
     * and hands a driver its own keys with the prefix stripped (see
     * software-docs/specs/2026-11-15-open-slide-options-design.md, section 5).*/
    using DriverOptions = std::map<std::string, std::string>;

    struct OptionKey
    {
        std::string driver;     ///< before the first '.'; empty when there is none
        std::string name;       ///< after the first '.'; the whole key when there is none
    };

    SLIDEIO_CORE_EXPORTS OptionKey splitOptionKey(const std::string& key);
    /**@brief the key a caller writes for a driver's option: lowercase id, '.', name.*/
    SLIDEIO_CORE_EXPORTS std::string optionKeyOf(const std::string& driverId, const std::string& name);
    /**@brief true/false/1/0 in any letter case; anything else throws, naming the key.*/
    SLIDEIO_CORE_EXPORTS bool parseBoolOption(const std::string& key, const std::string& value);
}
