// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/mrxs/mrxsini.hpp"
#include "slideio/drivers/mrxs/mrxserrors.hpp"

#include <charconv>
#include <cmath>
#include <fstream>
#include <locale>
#include <sstream>
#include <system_error>

using namespace slideio::mrxs;
namespace fs = std::filesystem;

namespace
{
    std::string trim(const std::string& text)
    {
        const char* blanks = " \t";
        const size_t first = text.find_first_not_of(blanks);
        if (first == std::string::npos) {
            return {};
        }
        const size_t last = text.find_last_not_of(blanks);
        return text.substr(first, last - first + 1);
    }

    bool isDigit(char c) { return c >= '0' && c <= '9'; }

    // [+-]? (digits [. digits?] | . digits) ([eE] [+-]? digits)?  -- the whole value.
    bool isDecimalNumber(const std::string& s)
    {
        size_t i = 0;
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
        size_t intDigits = 0, fracDigits = 0;
        while (i < s.size() && isDigit(s[i])) { ++i; ++intDigits; }
        if (i < s.size() && s[i] == '.') {
            ++i;
            while (i < s.size() && isDigit(s[i])) { ++i; ++fracDigits; }
        }
        if (intDigits + fracDigits == 0) return false;
        if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
            ++i;
            if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
            size_t expDigits = 0;
            while (i < s.size() && isDigit(s[i])) { ++i; ++expDigits; }
            if (expDigits == 0) return false;
        }
        return i == s.size();
    }
}

IniFile IniFile::parse(const std::string& text)
{
    IniFile ini;
    ini.m_text = text;
    size_t pos = text.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : 0;
    Group* current = nullptr;
    int lineNumber = 0;
    while (pos <= text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        line = trim(line);
        // Blank lines, comments, and the empty-key lines OpenSlide removes.
        if (line.empty() || line[0] == '#' || line[0] == '=') {
            continue;
        }
        if (line[0] == '[') {
            if (line.back() != ']' || line.size() < 3) {
                MRXS_CORRUPT << "Slidedat.ini line " << lineNumber << " is not a valid group header";
            }
            const std::string name = line.substr(1, line.size() - 2);
            current = nullptr;
            for (Group& group : ini.m_groups) {
                if (group.name == name) { current = &group; break; }     // repeated group: merge
            }
            if (!current) {
                ini.m_groups.push_back(Group{name, {}});
                current = &ini.m_groups.back();
            }
            continue;
        }
        const size_t equals = line.find('=');
        if (equals == std::string::npos) {
            MRXS_CORRUPT << "Slidedat.ini line " << lineNumber << " is neither a group nor a key";
        }
        if (!current) {
            MRXS_CORRUPT << "Slidedat.ini line " << lineNumber << " has a key outside any group";
        }
        const std::string key = trim(line.substr(0, equals));
        const std::string value = trim(line.substr(equals + 1));
        bool replaced = false;
        for (auto& entry : current->entries) {
            if (entry.first == key) { entry.second = value; replaced = true; break; }   // last value wins
        }
        if (!replaced) {
            current->entries.emplace_back(key, value);
        }
    }
    return ini;
}

IniFile IniFile::read(const fs::path& path)
{
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    if (ec) {
        MRXS_CORRUPT << "cannot read " << path.u8string() << ": " << ec.message();
    }
    if (size > kMaxBytes) {
        MRXS_LIMIT << path.u8string() << " is " << size << " bytes; Slidedat.ini is limited to "
            << kMaxBytes << " bytes";
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        MRXS_CORRUPT << "cannot open " << path.u8string();
    }
    std::string text(static_cast<size_t>(size), '\0');
    in.read(&text[0], static_cast<std::streamsize>(size));
    if (static_cast<uint64_t>(in.gcount()) != size) {
        MRXS_CORRUPT << "cannot read " << path.u8string();
    }
    return parse(text);
}

const std::string* IniFile::find(const std::string& group, const std::string& key) const
{
    for (const Group& g : m_groups) {
        if (g.name == group) {
            for (const auto& entry : g.entries) {
                if (entry.first == key) return &entry.second;
            }
            return nullptr;
        }
    }
    return nullptr;
}

const std::string& IniFile::require(const std::string& group, const std::string& key) const
{
    const std::string* value = find(group, key);
    if (!value) {
        MRXS_CORRUPT << "Slidedat.ini has no " << key << " in [" << group << "]";
    }
    return *value;
}

bool IniFile::hasGroup(const std::string& group) const
{
    for (const Group& g : m_groups) {
        if (g.name == group) return true;
    }
    return false;
}

int64_t slideio::mrxs::parseInteger(const std::string& value, const std::string& what)
{
    int64_t result = 0;
    const char* first = value.data();
    const char* last = value.data() + value.size();
    if (first != last && *first == '+') ++first;
    const auto [ptr, ec] = std::from_chars(first, last, result);
    if (value.empty() || ec != std::errc() || ptr != last) {
        MRXS_CORRUPT << what << " is not an integer: '" << value << "'";
    }
    return result;
}

double slideio::mrxs::parseReal(const std::string& value, const std::string& what)
{
    if (!isDecimalNumber(value)) {
        MRXS_CORRUPT << what << " is not a number: '" << value << "'";
    }
    std::istringstream in(value);
    in.imbue(std::locale::classic());
    double result = 0;
    in >> result;
    if (in.fail() || !std::isfinite(result)) {
        MRXS_CORRUPT << what << " is not a finite number: '" << value << "'";
    }
    return result;
}
