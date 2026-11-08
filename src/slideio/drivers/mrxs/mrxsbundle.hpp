// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/mrxs/mrxs_api_def.hpp"
#include <filesystem>
#include <string>

namespace slideio::mrxs
{
    /**@brief where the files of one MRXS bundle are.*/
    struct BundlePaths
    {
        std::filesystem::path entry;    ///< the path the caller opened
        std::filesystem::path dataDir;  ///< directory holding Slidedat.ini, index and data files
        std::filesystem::path ini;      ///< dataDir / Slidedat.ini
        std::string sceneName;          ///< stem of the data directory, UTF-8
    };

    /**@brief a native path from a UTF-8 string (on Windows a std::string path would be
     * read in the ANSI code page).*/
    SLIDEIO_MRXS_EXPORTS std::filesystem::path pathFromUtf8(const std::string& utf8);

    /**@brief true for a path the driver claims: `*.mrxs` or a file named Slidedat.ini,
     * both compared without regard to ASCII letter case. Does no I/O.*/
    SLIDEIO_MRXS_EXPORTS bool isEntryName(const std::string& filePath);

    /**@brief resolves an entry path (UTF-8) to its bundle.
     *
     * A `.mrxs` entry must exist and have a sibling directory of the same stem; a
     * Slidedat.ini entry names its own directory and needs no `.mrxs` file. Either way
     * Slidedat.ini must exist. Throws RuntimeError naming the missing piece.*/
    SLIDEIO_MRXS_EXPORTS BundlePaths resolveBundle(const std::string& filePath);
}
