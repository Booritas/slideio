// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/bif/bif_api_def.hpp"
#include <map>
#include <optional>
#include <string>

namespace slideio::bif
{
    struct SLIDEIO_BIF_EXPORTS LevelDescription
    {
        int level = 0;
        double magnification = 0.;
        std::map<std::string, std::string> tokens;
    };

    // Parses the ImageDescription of a pyramid level ("level=0 mag=40 quality=95").
    // Returns nullopt when there is no "level" token; throws when level is present
    // but mag is missing, or either value fails to parse.
    SLIDEIO_BIF_EXPORTS std::optional<LevelDescription> parseLevelDescription(const std::string& description);

    enum class AuxKind { None, Label, Thumbnail, Probability };

    // Exact, case-sensitive match of the ImageDescription of an auxiliary image.
    SLIDEIO_BIF_EXPORTS AuxKind classifyAuxDescription(const std::string& description);

    // "Label", "Thumbnail" or "Probability"; empty for AuxKind::None.
    SLIDEIO_BIF_EXPORTS std::string auxImageName(AuxKind kind);
}
