// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/core/metadata.hpp"
#include "slideio/base/slideio_enums.hpp"
#include <nlohmann/json.hpp>

namespace slideio
{
    namespace detail
    {

        SLIDEIO_CORE_EXPORTS Metadata makeMetadataFromJson(nlohmann::json root);
        SLIDEIO_CORE_EXPORTS nlohmann::json xmlStringToJson(const std::string& xml);
        SLIDEIO_CORE_EXPORTS MetadataBuilder builderFromJson(nlohmann::json root);
        SLIDEIO_CORE_EXPORTS MetadataBuilder makeDefaultMetadataBuilder(const std::string& rawMetadata,
                                                                        MetadataFormat fmt);

    } // namespace detail
} // namespace slideio
