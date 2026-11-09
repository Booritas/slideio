// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/bif/bifdescription.hpp"
#include "slideio/drivers/bif/bifnumeric.hpp"
#include "slideio/core/exceptions.hpp"

#include <sstream>

namespace slideio::bif
{
    namespace
    {
        int parseInt(const std::string& name, const std::string& value)
        {
            return detail::parseIntStrict(name, value);
        }

        double parseDouble(const std::string& name, const std::string& value)
        {
            return detail::parseDoubleStrict(name, value);
        }
    }

    std::optional<LevelDescription> parseLevelDescription(const std::string& description)
    {
        std::map<std::string, std::string> tokens;
        std::istringstream stream(description);
        std::string word;
        while (stream >> word) {
            const size_t eq = word.find('=');
            if (eq == std::string::npos) {
                continue;
            }
            tokens[word.substr(0, eq)] = word.substr(eq + 1);
        }
        const auto levelIt = tokens.find("level");
        if (levelIt == tokens.end()) {
            return std::nullopt;
        }
        const auto magIt = tokens.find("mag");
        if (magIt == tokens.end()) {
            RAISE_RUNTIME_ERROR << "BIF: level description has no mag token: '" << description << "'";
        }
        LevelDescription result;
        result.level = parseInt("level", levelIt->second);
        result.magnification = parseDouble("mag", magIt->second);
        result.tokens = std::move(tokens);
        return result;
    }

    AuxKind classifyAuxDescription(const std::string& description)
    {
        if (description == "Label Image" || description == "Label_Image") {
            return AuxKind::Label;
        }
        if (description == "Thumbnail") {
            return AuxKind::Thumbnail;
        }
        if (description == "Probability_Image") {
            return AuxKind::Probability;
        }
        return AuxKind::None;
    }

    std::string auxImageName(AuxKind kind)
    {
        switch (kind) {
        case AuxKind::Label: return "Label";
        case AuxKind::Thumbnail: return "Thumbnail";
        case AuxKind::Probability: return "Probability";
        default: return std::string();
        }
    }
}
