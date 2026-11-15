// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/openoptions.hpp"
#include "slideio/core/exceptions.hpp"

#include <algorithm>
#include <cctype>

namespace
{
    std::string lowercase(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }
}

slideio::OptionKey slideio::splitOptionKey(const std::string& key)
{
    const size_t dot = key.find('.');
    if (dot == std::string::npos) {
        return {std::string(), key};
    }
    return {key.substr(0, dot), key.substr(dot + 1)};
}

std::string slideio::optionKeyOf(const std::string& driverId, const std::string& name)
{
    return lowercase(driverId) + "." + name;
}

bool slideio::parseBoolOption(const std::string& key, const std::string& value)
{
    const std::string v = lowercase(value);
    if (v == "true" || v == "1") {
        return true;
    }
    if (v == "false" || v == "0") {
        return false;
    }
    RAISE_RUNTIME_ERROR << "open option '" << key << "': '" << value
        << "' is not a boolean (true/false/1/0)";
}
