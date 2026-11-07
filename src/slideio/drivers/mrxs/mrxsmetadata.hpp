// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/mrxs/mrxs_api_def.hpp"
#include "slideio/drivers/mrxs/mrxsini.hpp"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace slideio::mrxs
{
    enum class ImageFormat { Jpeg, Png, Bmp24 };

    /**@brief one stored pyramid level, from its section of Slidedat.ini.*/
    struct LevelMetadata
    {
        int exponent = 0;           ///< IMAGE_CONCAT_FACTOR, an exponent increment
        int64_t concat = 1;         ///< C_l = 2^(e_0 + ... + e_l)
        int imageWidth = 0;         ///< DIGITIZER_WIDTH, W_l
        int imageHeight = 0;        ///< DIGITIZER_HEIGHT, H_l
        double overlapX = 0;
        double overlapY = 0;
        double mppX = 0;            ///< micrometres per pixel
        double mppY = 0;
        ImageFormat format = ImageFormat::Jpeg;
        int64_t fillColorBGR = 0;   ///< low byte red, then green, then blue
        std::string section;
    };

    struct FamilyValue
    {
        std::string name;
        std::string section;        ///< INI group describing the value; may be empty
    };

    struct Family
    {
        std::string name;
        std::vector<FamilyValue> values;
    };

    /**@brief everything Slidedat.ini says that reading the slide needs, validated.*/
    struct SlideMetadata
    {
        std::string slideId;
        int nx = 0;                 ///< IMAGENUMBER_X
        int ny = 0;                 ///< IMAGENUMBER_Y
        int d = 1;                  ///< CameraImageDivisionsPerSide
        std::optional<double> objective;
        std::vector<LevelMetadata> levels;
        std::filesystem::path indexFile;
        std::vector<std::filesystem::path> dataFiles;
        std::vector<Family> hierarchical;
        std::vector<Family> nonHierarchical;
    };

    /**@brief extracts and validates the metadata. File names are resolved against
     * dataDir and must stay inside it. Throws MRXS corrupt or unsupported errors.*/
    SLIDEIO_MRXS_EXPORTS SlideMetadata readMetadata(const IniFile& ini, const std::filesystem::path& dataDir);
}
