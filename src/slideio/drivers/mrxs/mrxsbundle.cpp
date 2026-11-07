// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/mrxs/mrxsbundle.hpp"
#include "slideio/drivers/mrxs/mrxserrors.hpp"
#include "slideio/core/tools/tools.hpp"

#include <cctype>
#include <system_error>

namespace fs = std::filesystem;

namespace
{
    std::string asciiLower(std::string value)
    {
        for (char& c : value) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return value;
    }

    bool isMrxsExtension(const fs::path& path)
    {
        return asciiLower(path.extension().u8string()) == ".mrxs";
    }

    bool isSlidedatIni(const fs::path& path)
    {
        return asciiLower(path.filename().u8string()) == "slidedat.ini";
    }

    bool isRegularFile(const fs::path& path)
    {
        std::error_code ec;
        return fs::is_regular_file(path, ec);
    }

    bool isDirectory(const fs::path& path)
    {
        std::error_code ec;
        return fs::is_directory(path, ec);
    }
}

fs::path slideio::mrxs::pathFromUtf8(const std::string& utf8)
{
#if defined(WIN32)
    return fs::path(slideio::Tools::toWstring(utf8));
#else
    return fs::path(utf8);
#endif
}

bool slideio::mrxs::isEntryName(const std::string& filePath)
{
    const fs::path path = pathFromUtf8(filePath);
    return isMrxsExtension(path) || isSlidedatIni(path);
}

slideio::mrxs::BundlePaths slideio::mrxs::resolveBundle(const std::string& filePath)
{
    BundlePaths paths;
    paths.entry = pathFromUtf8(filePath);
    if (isSlidedatIni(paths.entry)) {
        if (!isRegularFile(paths.entry)) {
            MRXS_CORRUPT << filePath << " does not exist";
        }
        paths.dataDir = paths.entry.parent_path();
        paths.ini = paths.entry;
    }
    else if (isMrxsExtension(paths.entry)) {
        if (!isRegularFile(paths.entry)) {
            MRXS_CORRUPT << "entry file " << filePath << " does not exist";
        }
        // The data directory is the entry's sibling with the same stem, whatever the
        // case of the .mrxs suffix.
        paths.dataDir = paths.entry.parent_path() / paths.entry.stem();
        if (!isDirectory(paths.dataDir)) {
            MRXS_CORRUPT << "entry file " << filePath << " has no data directory "
                << paths.dataDir.u8string();
        }
        paths.ini = paths.dataDir / "Slidedat.ini";
        if (!isRegularFile(paths.ini)) {
            MRXS_CORRUPT << "data directory " << paths.dataDir.u8string() << " has no Slidedat.ini";
        }
    }
    else {
        MRXS_UNSUPPORTED << filePath << " is neither a .mrxs entry file nor Slidedat.ini";
    }
    paths.sceneName = paths.dataDir.filename().u8string();
    return paths;
}
