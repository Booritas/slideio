// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/mrxs/mrxs_api_def.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace slideio::mrxs
{
    /**@brief Slidedat.ini, parsed with GLib GKeyFile semantics.
     *
     * Groups in square brackets, key = value lines, '#' comments. A leading UTF-8
     * byte-order mark and lines whose first non-blank character is '=' are dropped,
     * as OpenSlide does. Names are case-sensitive. A repeated key keeps its last value;
     * a repeated group merges into its first occurrence (both confirmed against
     * OpenSlide 4.0.1). Values are kept as text; parseInteger and parseReal convert
     * them without regard to the C locale.*/
    class SLIDEIO_MRXS_EXPORTS IniFile
    {
    public:
        /// OpenSlide's limit, kept for compatibility (design section 4).
        static constexpr std::size_t kMaxBytes = 1u << 20;

        struct Group
        {
            std::string name;
            std::vector<std::pair<std::string, std::string>> entries;
        };

        static IniFile parse(const std::string& text);
        /// Reads at most kMaxBytes; a larger file is a limit error.
        static IniFile read(const std::filesystem::path& path);

        /// The value, or nullptr when the group or key is absent.
        const std::string* find(const std::string& group, const std::string& key) const;
        /// The value; a missing key is a corrupt-bundle error naming group and key.
        const std::string& require(const std::string& group, const std::string& key) const;
        bool hasGroup(const std::string& group) const;
        const std::vector<Group>& groups() const { return m_groups; }
        /// The file exactly as read, byte-order mark and line endings included.
        const std::string& text() const { return m_text; }

    private:
        std::vector<Group> m_groups;
        std::string m_text;
    };

    /// A decimal integer filling the whole value; anything else is corrupt.
    SLIDEIO_MRXS_EXPORTS int64_t parseInteger(const std::string& value, const std::string& what);
    /// A finite decimal number filling the whole value, '.' as separator in every locale.
    SLIDEIO_MRXS_EXPORTS double parseReal(const std::string& value, const std::string& what);
}
