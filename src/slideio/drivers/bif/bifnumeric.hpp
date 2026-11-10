// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/core/exceptions.hpp"
#include <cctype>
#include <locale>
#include <sstream>
#include <string>

// Internal helpers (not exported). Strict: the whole value, bar surrounding whitespace,
// must be a number. Locale independent: the process-wide C/C++ locale is ignored.
namespace slideio::bif::detail
{
    template <typename T>
    bool tryParseNumber(const std::string& text, T& result)
    {
        size_t begin = 0;
        size_t end = text.size();
        while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) {
            ++begin;
        }
        while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
            --end;
        }
        if (begin == end) {
            return false;
        }
        std::istringstream stream(text.substr(begin, end - begin));
        stream.imbue(std::locale::classic());
        T value{};
        stream >> value;
        if (stream.fail() || stream.peek() != std::char_traits<char>::eof()) {
            return false;
        }
        result = value;
        return true;
    }

    inline int parseIntStrict(const std::string& name, const std::string& value)
    {
        int result = 0;
        if (!tryParseNumber(value, result)) {
            RAISE_RUNTIME_ERROR << "BIF: cannot parse " << name << " value '" << value << "'";
        }
        return result;
    }

    inline double parseDoubleStrict(const std::string& name, const std::string& value)
    {
        double result = 0.;
        if (!tryParseNumber(value, result)) {
            RAISE_RUNTIME_ERROR << "BIF: cannot parse " << name << " value '" << value << "'";
        }
        return result;
    }
}
