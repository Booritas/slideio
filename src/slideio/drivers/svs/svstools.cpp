// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/svs/svstools.hpp"
#include "slideio/core/slideio_enums.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/core/log.hpp"
#include <cmath>
#include <locale>
#include <string>
#include <regex>
#include <sstream>
#include <vector>

using namespace slideio;

namespace {
    std::string trimWS(const std::string& s) {
        const size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return {};
        const size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }

    template <typename T>
    std::string enumToString(const T& value) {
        std::ostringstream os;
        os << value;
        return os.str();
    }
}

int SVSTools::extractMagnifiation(const std::string& description)
{
    int magn = 0;
    std::regex rgx("\\|AppMag\\s=\\s(\\d+)\\|");
    std::smatch match;
    if(std::regex_search(description, match, rgx)){
        std::string magn_str = match[1];
        magn = std::stoi(magn_str);
    }
    return magn;
}

double SVSTools::extractResolution(const std::string& description)
{
    double res = 0;
    std::regex rgx(R"(\|MPP\s=\s((\d*(\.|,))?(\d+)?))");
    std::smatch match;
    if (std::regex_search(description, match, rgx)) {
        std::string res_str = match[1];
        std::replace(res_str.begin(), res_str.end(), ',', '.');
        res = std::stod(res_str) * 1.e-6;
    }
    return res;
}

double SVSTools::extractPhilipsMagnification(const std::string& description)
{
    // The largest level number a pyramid can reach: scaling by more than this overflows
    // and nothing in a real file comes close (the deepest of the test files is 9).
    const int MAX_LEVEL = 30;
    // The level number is bounded in the pattern rather than after the fact, so that a
    // corrupt description carrying a run of digits no int can hold simply fails to match
    // instead of raising out of the conversion.
    std::regex rgx(R"(level=(\d{1,9})\s+mag=(\d+(?:\.\d+)?))");
    std::smatch match;
    if (!std::regex_search(description, match, rgx)) {
        return 0.;
    }
    const int level = std::stoi(match[1]);
    if (level < 0 || level > MAX_LEVEL) {
        return 0.;
    }
    // The value comes out of a file, so it must not be read through the locale the
    // embedding application happens to have set: a comma decimal locale would stop
    // "5.5" at the point and turn a 44x slide into a 40x one.
    std::istringstream stream(match[2].str());
    stream.imbue(std::locale::classic());
    double magnification = 0.;
    stream >> magnification;
    if (!stream || magnification <= 0.) {
        return 0.;
    }
    return std::ldexp(magnification, level);
}

int SVSTools::extractPhilipsLevelNumber(const std::string& description)
{
    // The same shape the magnification parser trusts, and deliberately not a bare
    // "level=(\d+)": the derivation description of a converted file contains
    // "levels=10003,10002", and requiring the mag field keeps that from matching.
    std::regex rgx(R"(level=(\d{1,9})\s+mag=)");
    std::smatch match;
    if (!std::regex_search(description, match, rgx)) {
        return -1;
    }
    return std::stoi(match[1]);
}

namespace
{
    bool allDigitsSvs(const std::string& text)
    {
        if (text.empty()) {
            return false;
        }
        for (const char c : text) {
            if (c < '0' || c > '9') {
                return false;
            }
        }
        return true;
    }
}

std::optional<int64_t> SVSTools::aperioDateTimeToEpochSeconds(const std::string& date,
                                                              const std::string& time,
                                                              const std::string& zone)
{
    // MM/DD/YY or MM/DD/YYYY.
    if ((date.size() != 8 && date.size() != 10) || date[2] != '/' || date[5] != '/') {
        return std::nullopt;
    }
    const std::string month = date.substr(0, 2);
    const std::string day = date.substr(3, 2);
    std::string year = date.substr(6);
    if (!allDigitsSvs(month) || !allDigitsSvs(day) || !allDigitsSvs(year)) {
        return std::nullopt;
    }
    if (year.size() == 2) {
        // The window strptime's %y uses. A fixed rule rather than one that
        // slides with today's date, so the same file reads the same next
        // decade.
        year = (std::stoi(year) <= 68 ? "20" : "19") + year;
    }

    // HH:MM:SS.
    if (time.size() != 8 || time[2] != ':' || time[5] != ':') {
        return std::nullopt;
    }
    if (!allDigitsSvs(time.substr(0, 2)) || !allDigitsSvs(time.substr(3, 2))
        || !allDigitsSvs(time.substr(6, 2))) {
        return std::nullopt;
    }

    std::string iso = year + "-" + month + "-" + day + "T" + time;
    if (zone.empty()) {
        iso += "Z";
    }
    else {
        // "GMT-05:00", or the same without the colon. A zone that cannot be
        // read falls back to UTC rather than discarding the Date and Time,
        // which were perfectly readable -- the same reading an absent zone
        // gets, and strictly more than reporting nothing.
        std::string digits = (zone.size() > 4) ? zone.substr(4) : std::string();
        const size_t colon = digits.find(':');
        if (colon != std::string::npos) {
            digits.erase(colon, 1);
        }
        if (zone.size() < 6 || zone.compare(0, 3, "GMT") != 0
            || (zone[3] != '+' && zone[3] != '-')
            || digits.size() != 4 || !allDigitsSvs(digits)) {
            iso += "Z";
        }
        else {
            iso += zone[3];
            iso += digits.substr(0, 2);
            iso += ":";
            iso += digits.substr(2, 2);
        }
    }
    const auto epoch = Tools::parseIso8601(iso);
    if (!epoch) {
        return std::nullopt;
    }
    // Whole seconds; an Aperio time states none beyond them.
    return static_cast<int64_t>(std::floor(*epoch));
}

nlohmann::json SVSTools::parseAperioMetadata(const std::string& description)
{
    using nlohmann::json;
    json result = json::object();

    const size_t firstPipe = description.find('|');
    const std::string header = (firstPipe == std::string::npos)
        ? description
        : description.substr(0, firstPipe);

    std::vector<std::string> headerLines;
    {
        std::string current;
        for (char c : header) {
            if (c == '\n') {
                std::string t = trimWS(current);
                if (!t.empty()) headerLines.push_back(std::move(t));
                current.clear();
            } else if (c != '\r') {
                current += c;
            }
        }
        std::string t = trimWS(current);
        if (!t.empty()) headerLines.push_back(std::move(t));
    }

    if (headerLines.size() >= 1) result["application"] = headerLines[0];
    if (headerLines.size() >= 2) result["image"]       = headerLines[1];

    if (firstPipe != std::string::npos) {
        json props = json::object();
        size_t cursor = firstPipe + 1;
        while (cursor <= description.size()) {
            const size_t next = description.find('|', cursor);
            const std::string token = (next == std::string::npos)
                ? description.substr(cursor)
                : description.substr(cursor, next - cursor);

            const size_t eq = token.find('=');
            if (eq != std::string::npos) {
                const std::string name  = trimWS(token.substr(0, eq));
                const std::string value = trimWS(token.substr(eq + 1));
                if (!name.empty()) props[name] = value;
            }

            if (next == std::string::npos) break;
            cursor = next + 1;
        }
        result["properties"] = std::move(props);
    }

    return result;
}

int SVSTools::significantBitsFromDescription(const std::string& description)
{
    const nlohmann::json metadata = parseAperioMetadata(description);
    const auto props = metadata.find("properties");
    if (props == metadata.end() || !props->is_object()) {
        return 0;
    }
    const auto it = props->find("Acquisition Bit Depth");
    if (it == props->end() || !it->is_string()) {
        return 0;
    }
    try {
        const int bits = std::stoi(it->get<std::string>());
        // 0 is what the getter means by unknown, so a value that cannot be one
        // reports unknown rather than itself.
        return (bits > 0) ? bits : 0;
    }
    catch (const std::exception&) {
        return 0;
    }
}

int64_t SVSTools::acquisitionTimeFromDescription(const std::string& description)
{
    const nlohmann::json metadata = parseAperioMetadata(description);
    const auto props = metadata.find("properties");
    if (props == metadata.end() || !props->is_object()) {
        return 0;
    }
    const auto readProp = [&props](const char* name) -> std::string {
        const auto it = props->find(name);
        return (it != props->end() && it->is_string()) ? it->get<std::string>() : std::string();
    };
    const std::string date = readProp("Date");
    const std::string time = readProp("Time");
    if (date.empty() || time.empty()) {
        // Not every svs carries an Aperio header, and not every header states
        // the scan time. Absent is not an error.
        return 0;
    }
    if (const auto epoch = aperioDateTimeToEpochSeconds(date, time, readProp("Time Zone"))) {
        return *epoch;
    }
    // Stated but unreadable is not the same as absent, and both report 0.
    SLIDEIO_LOG(WARNING) << "SVSImageDriver: unreadable Aperio scan time '" << date
        << " " << time << "' zone '" << readProp("Time Zone") << "'";
    return 0;
}

nlohmann::json SVSTools::tiffDirectoryToJson(const TiffDirectory& dir)
{
    using nlohmann::json;
    json j;
    j["dirIndex"] = dir.dirIndex;
    j["offset"] = dir.offset;
    j["width"] = dir.width;
    j["height"] = dir.height;
    j["tiled"] = dir.tiled;
    j["tileWidth"] = dir.tileWidth;
    j["tileHeight"] = dir.tileHeight;
    j["channels"] = dir.channels;
    j["bitsPerSample"] = dir.bitsPerSample;
    j["photometric"] = dir.photometric;
    j["YCbCrSubsampling"] = { dir.YCbCrSubsampling[0], dir.YCbCrSubsampling[1] };
    j["subFileType"] = dir.subFileType;
    j["compression"] = dir.compression;
    j["slideioCompression"] = enumToString(dir.slideioCompression);
    j["description"] = dir.description;
    j["software"] = dir.software;
    j["resolution"] = { {"x", dir.res.x}, {"y", dir.res.y} };
    j["position"] = { {"x", dir.position.x}, {"y", dir.position.y} };
    j["interleaved"] = dir.interleaved;
    j["rowsPerStrip"] = dir.rowsPerStrip;
    j["dataType"] = enumToString(dir.dataType);
    j["stripSize"] = dir.stripSize;
    j["compressionQuality"] = dir.compressionQuality;
    j["byteOffset"] = dir.byteOffset;

    if (!dir.subdirectories.empty()) {
        auto subs = json::array();
        for (const auto& sub : dir.subdirectories) {
            subs.push_back(tiffDirectoryToJson(sub));
        }
        j["subdirectories"] = subs;
    }
    return j;
}
