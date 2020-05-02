// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/svs/svstools.hpp"
#include <string>
#include <regex>

using namespace slideio;

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
