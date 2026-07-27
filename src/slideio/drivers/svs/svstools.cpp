// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/svs/svstools.hpp"
#include "slideio/base/slideio_enums.hpp"
#include <string>
#include <regex>
#include <sstream>
#include <vector>

using namespace slideio;

namespace
{
    std::string trimWS(const std::string& s)
    {
        const size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return {};
        const size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }

    template <typename T> std::string enumToString(const T& value)
    {
        std::ostringstream os;
        os << value;
        return os.str();
    }
} // namespace

int SVSTools::extractMagnifiation(const std::string& description)
{
    int magn = 0;
    std::regex rgx("\\|AppMag\\s=\\s(\\d+)\\|");
    std::smatch match;
    if (std::regex_search(description, match, rgx))
    {
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
    if (std::regex_search(description, match, rgx))
    {
        std::string res_str = match[1];
        std::replace(res_str.begin(), res_str.end(), ',', '.');
        res = std::stod(res_str) * 1.e-6;
    }
    return res;
}

nlohmann::json SVSTools::parseAperioMetadata(const std::string& description)
{
    using nlohmann::json;
    json result = json::object();

    const size_t firstPipe = description.find('|');
    const std::string header = (firstPipe == std::string::npos) ? description : description.substr(0, firstPipe);

    std::vector<std::string> headerLines;
    {
        std::string current;
        for (char c : header)
        {
            if (c == '\n')
            {
                std::string t = trimWS(current);
                if (!t.empty()) headerLines.push_back(std::move(t));
                current.clear();
            }
            else if (c != '\r')
            {
                current += c;
            }
        }
        std::string t = trimWS(current);
        if (!t.empty()) headerLines.push_back(std::move(t));
    }

    if (headerLines.size() >= 1) result["application"] = headerLines[0];
    if (headerLines.size() >= 2) result["image"] = headerLines[1];

    if (firstPipe != std::string::npos)
    {
        json props = json::object();
        size_t cursor = firstPipe + 1;
        while (cursor <= description.size())
        {
            const size_t next = description.find('|', cursor);
            const std::string token =
                (next == std::string::npos) ? description.substr(cursor) : description.substr(cursor, next - cursor);

            const size_t eq = token.find('=');
            if (eq != std::string::npos)
            {
                const std::string name = trimWS(token.substr(0, eq));
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
    j["YCbCrSubsampling"] = {dir.YCbCrSubsampling[0], dir.YCbCrSubsampling[1]};
    j["subFileType"] = dir.subFileType;
    j["compression"] = dir.compression;
    j["slideioCompression"] = enumToString(dir.slideioCompression);
    j["description"] = dir.description;
    j["software"] = dir.software;
    j["resolution"] = {{"x", dir.res.x}, {"y", dir.res.y}};
    j["position"] = {{"x", dir.position.x}, {"y", dir.position.y}};
    j["interleaved"] = dir.interleaved;
    j["rowsPerStrip"] = dir.rowsPerStrip;
    j["dataType"] = enumToString(dir.dataType);
    j["stripSize"] = dir.stripSize;
    j["compressionQuality"] = dir.compressionQuality;
    j["byteOffset"] = dir.byteOffset;

    if (!dir.subdirectories.empty())
    {
        auto subs = json::array();
        for (const auto& sub : dir.subdirectories)
            subs.push_back(tiffDirectoryToJson(sub));
        j["subdirectories"] = subs;
    }
    return j;
}
