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
    size_t current = std::string::npos;     // index into m_groups
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
            const auto inserted = ini.m_groupIndex.emplace(name, ini.m_groups.size());
            current = inserted.first->second;       // repeated group: merge
            if (inserted.second) {
                ini.m_groups.push_back(Group{name, {}});
                ini.m_keyIndex.emplace_back();
            }
            continue;
        }
        const size_t equals = line.find('=');
        if (equals == std::string::npos) {
            MRXS_CORRUPT << "Slidedat.ini line " << lineNumber << " is neither a group nor a key";
        }
        if (current == std::string::npos) {
            MRXS_CORRUPT << "Slidedat.ini line " << lineNumber << " has a key outside any group";
        }
        const std::string key = trim(line.substr(0, equals));
        const std::string value = trim(line.substr(equals + 1));
        auto& entries = ini.m_groups[current].entries;
        const auto inserted = ini.m_keyIndex[current].emplace(key, entries.size());
        if (inserted.second) {
            entries.emplace_back(key, value);
        }
        else {
            entries[inserted.first->second].second = value;     // last value wins
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
    const auto g = m_groupIndex.find(group);
    if (g == m_groupIndex.end()) {
        return nullptr;
    }
    const auto k = m_keyIndex[g->second].find(key);
    return k == m_keyIndex[g->second].end() ? nullptr : &m_groups[g->second].entries[k->second].second;
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
    return m_groupIndex.count(group) != 0;
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
