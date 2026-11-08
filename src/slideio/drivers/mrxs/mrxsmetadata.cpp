// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/mrxs/mrxsmetadata.hpp"
#include "slideio/drivers/mrxs/mrxsbundle.hpp"
#include "slideio/drivers/mrxs/mrxserrors.hpp"

#include <limits>
#include <set>
#include <system_error>

using namespace slideio::mrxs;
namespace fs = std::filesystem;

namespace
{
    const char* kGeneral = "GENERAL";
    const char* kHierarchical = "HIERARCHICAL";
    const char* kDataFile = "DATAFILE";
    // OpenSlide's limit: C_l must fit a signed 32-bit concatenation factor.
    const int kMaxCumulativeExponent = 30;

    std::string key(const char* prefix, int i, const char* suffix = "")
    {
        return std::string(prefix) + std::to_string(i) + suffix;
    }

    std::string valueKey(const char* family, int i, int j, const char* suffix = "")
    {
        return std::string(family) + std::to_string(i) + "_VAL_" + std::to_string(j) + suffix;
    }

    int positiveInt(const IniFile& ini, const char* group, const std::string& name)
    {
        const int64_t value = parseInteger(ini.require(group, name), name);
        if (value <= 0 || value > std::numeric_limits<int32_t>::max()) {
            MRXS_CORRUPT << name << " must be a positive 32-bit integer, not " << value;
        }
        return static_cast<int>(value);
    }

    int countOf(const IniFile& ini, const std::string& name)
    {
        const int64_t value = parseInteger(ini.require(kHierarchical, name), name);
        if (value < 0 || value > std::numeric_limits<int32_t>::max()) {
            MRXS_CORRUPT << name << " must be a non-negative 32-bit integer, not " << value;
        }
        return static_cast<int>(value);
    }

    std::string optional(const IniFile& ini, const std::string& group, const std::string& name)
    {
        const std::string* value = ini.find(group, name);
        return value ? *value : std::string();
    }

    std::vector<Family> readFamilies(const IniFile& ini, const char* prefix, const char* countKey)
    {
        std::vector<Family> families;
        const int count = countOf(ini, countKey);
        for (int i = 0; i < count; ++i) {
            Family family;
            family.name = ini.require(kHierarchical, key(prefix, i, "_NAME"));
            const int values = countOf(ini, key(prefix, i, "_COUNT"));
            for (int j = 0; j < values; ++j) {
                family.values.push_back(FamilyValue{optional(ini, kHierarchical, valueKey(prefix, i, j)),
                                                    optional(ini, kHierarchical, valueKey(prefix, i, j, "_SECTION"))});
            }
            families.push_back(std::move(family));
        }
        return families;
    }

    // A file named in Slidedat.ini, resolved against the bundle directory. It must stay
    // inside it, lexically and after following symbolic links (design section 4).
    fs::path containedPath(const fs::path& dataDir, const std::string& name, const std::string& what)
    {
        const fs::path relative = pathFromUtf8(name);
        if (name.empty() || relative.has_root_name() || relative.has_root_directory()) {
            MRXS_CORRUPT << what << " '" << name << "' is not a relative file name";
        }
        const auto escapes = [](const fs::path& inner, const fs::path& outer) {
            const fs::path rel = inner.lexically_relative(outer);
            return rel.empty() || *rel.begin() == "..";
        };
        const fs::path base = dataDir.lexically_normal();
        const fs::path joined = (base / relative).lexically_normal();
        if (escapes(joined, base)) {
            MRXS_CORRUPT << what << " '" << name << "' leads outside the bundle directory";
        }
        std::error_code ecJoined, ecBase;
        const fs::path realJoined = fs::weakly_canonical(joined, ecJoined);
        const fs::path realBase = fs::weakly_canonical(base, ecBase);
        if (!ecJoined && !ecBase && escapes(realJoined, realBase)) {
            MRXS_CORRUPT << what << " '" << name << "' leads outside the bundle directory through a link";
        }
        return joined;
    }

    ImageFormat formatOf(const std::string& value, const std::string& section)
    {
        if (value == "JPEG") return ImageFormat::Jpeg;
        if (value == "PNG") return ImageFormat::Png;
        if (value == "BMP24") return ImageFormat::Bmp24;
        MRXS_UNSUPPORTED << "[" << section << "] IMAGE_FORMAT " << value << " (supported: JPEG, PNG, BMP24)";
    }

    std::optional<double> objectiveOf(const IniFile& ini)
    {
        // Informational: an invalid value leaves the objective unknown (design section 4).
        const std::string* value = ini.find(kGeneral, "OBJECTIVE_MAGNIFICATION");
        if (!value) {
            return std::nullopt;
        }
        std::string text = *value;
        if (!text.empty() && text.back() == 'x') {
            text.pop_back();
        }
        try {
            const double objective = parseReal(text, "OBJECTIVE_MAGNIFICATION");
            return objective > 0 ? std::optional<double>(objective) : std::nullopt;
        }
        catch (const slideio::RuntimeError&) {
            return std::nullopt;
        }
    }

    // A bundle storing its filters in more than one packed image group, such as the
    // five-channel mIHC sample, cannot be shown completely as one RGB pyramid.
    void rejectSeveralImageGroups(const IniFile& ini, const std::vector<Family>& families)
    {
        for (const Family& family : families) {
            if (family.name != "Slide filter level") {
                continue;
            }
            std::set<std::string> groups;
            for (const FamilyValue& value : family.values) {
                if (const std::string* group = ini.find(value.section, "DATA_IN_THIS_FILTER_LEVEL")) {
                    groups.insert(*group);
                }
            }
            if (groups.size() > 1) {
                MRXS_UNSUPPORTED << "the slide stores its " << family.values.size() << " filters in "
                    << groups.size() << " image groups; only single-group (RGB) bundles are supported";
            }
        }
    }
}

SlideMetadata slideio::mrxs::readMetadata(const IniFile& ini, const fs::path& dataDir)
{
    SlideMetadata meta;
    meta.slideId = ini.require(kGeneral, "SLIDE_ID");
    meta.nx = positiveInt(ini, kGeneral, "IMAGENUMBER_X");
    meta.ny = positiveInt(ini, kGeneral, "IMAGENUMBER_Y");
    if (static_cast<int64_t>(meta.nx) * meta.ny > std::numeric_limits<int32_t>::max()) {
        MRXS_UNSUPPORTED << "the image grid " << meta.nx << " x " << meta.ny << " exceeds 32-bit image indices";
    }
    // Absent means one division (an old bundle); a malformed value is an error rather
    // than silently changed geometry, unlike OpenSlide (format description 4.1).
    if (ini.find(kGeneral, "CameraImageDivisionsPerSide")) {
        meta.d = positiveInt(ini, kGeneral, "CameraImageDivisionsPerSide");
    }
    if (meta.nx % meta.d != 0 || meta.ny % meta.d != 0) {
        MRXS_UNSUPPORTED << "image grid " << meta.nx << " x " << meta.ny
            << " is not a whole number of " << meta.d << " x " << meta.d << " camera blocks";
    }
    meta.objective = objectiveOf(ini);

    meta.hierarchical = readFamilies(ini, "HIER_", "HIER_COUNT");
    meta.nonHierarchical = readFamilies(ini, "NONHIER_", "NONHIER_COUNT");
    if (meta.hierarchical.empty() || meta.hierarchical[0].name != "Slide zoom level") {
        MRXS_UNSUPPORTED << "the first hierarchical family is not 'Slide zoom level'";
    }
    if (meta.hierarchical[0].values.empty()) {
        MRXS_CORRUPT << "the slide has no zoom levels";
    }
    rejectSeveralImageGroups(ini, meta.hierarchical);

    int cumulative = 0;
    for (size_t l = 0; l < meta.hierarchical[0].values.size(); ++l) {
        const std::string& section = meta.hierarchical[0].values[l].section;
        if (section.empty() || !ini.hasGroup(section)) {
            MRXS_CORRUPT << "zoom level " << l << " has no section in Slidedat.ini";
        }
        LevelMetadata level;
        level.section = section;
        const int64_t exponent = parseInteger(ini.require(section, "IMAGE_CONCAT_FACTOR"), "IMAGE_CONCAT_FACTOR");
        if ((l == 0 && exponent < 0) || (l > 0 && exponent <= 0)) {
            MRXS_CORRUPT << "zoom level " << l << " has IMAGE_CONCAT_FACTOR " << exponent
                << " (the first must be >= 0, later ones > 0)";
        }
        if (cumulative + exponent > kMaxCumulativeExponent) {
            MRXS_CORRUPT << "zoom level " << l << " has a concatenation factor beyond 2^" << kMaxCumulativeExponent;
        }
        cumulative += static_cast<int>(exponent);
        level.exponent = static_cast<int>(exponent);
        level.concat = int64_t(1) << cumulative;
        if (level.concat % meta.d != 0 && meta.d % level.concat != 0) {
            MRXS_UNSUPPORTED << "zoom level " << l << ": concatenation factor " << level.concat
                << " and camera divisions " << meta.d << " do not divide one another";
        }
        level.imageWidth = positiveInt(ini, section.c_str(), "DIGITIZER_WIDTH");
        level.imageHeight = positiveInt(ini, section.c_str(), "DIGITIZER_HEIGHT");
        level.overlapX = parseReal(ini.require(section, "OVERLAP_X"), "OVERLAP_X");
        level.overlapY = parseReal(ini.require(section, "OVERLAP_Y"), "OVERLAP_Y");
        if (level.overlapX < 0 || level.overlapY < 0) {
            MRXS_CORRUPT << "zoom level " << l << " has a negative overlap";
        }
        level.mppX = parseReal(ini.require(section, "MICROMETER_PER_PIXEL_X"), "MICROMETER_PER_PIXEL_X");
        level.mppY = parseReal(ini.require(section, "MICROMETER_PER_PIXEL_Y"), "MICROMETER_PER_PIXEL_Y");
        if (level.mppX <= 0 || level.mppY <= 0) {
            MRXS_CORRUPT << "zoom level " << l << " has a non-positive pixel size";
        }
        level.format = formatOf(ini.require(section, "IMAGE_FORMAT"), section);
        // Absent means white (design section 7); present but malformed is still corrupt.
        if (const std::string* fill = ini.find(section, "IMAGE_FILL_COLOR_BGR")) {
            level.fillColorBGR = parseInteger(*fill, "IMAGE_FILL_COLOR_BGR");
        }
        meta.levels.push_back(level);
    }

    meta.indexFile = containedPath(dataDir, ini.require(kHierarchical, "INDEXFILE"), "INDEXFILE");
    const int files = positiveInt(ini, kDataFile, "FILE_COUNT");
    for (int i = 0; i < files; ++i) {
        const std::string name = key("FILE_", i);
        meta.dataFiles.push_back(containedPath(dataDir, ini.require(kDataFile, name), name));
    }
    return meta;
}
