// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/svs/svsslide.hpp"
#include "slideio/drivers/svs/phtmetadata.hpp"

namespace slideio
{
    /**@brief One zoom level of a philips tiff pyramid: the tiff directory holding the
     * raster and the level number the philips metadata assigns to it. The level number
     * is needed on its own because it cannot be recovered from the directory: philips
     * pads every level up to a whole number of tiles, so two consecutive levels may end
     * up with the same stored width (levels 8 and 9 of Philips-2.tiff are both 512).*/
    struct PHTLevel
    {
        int dirIndex = 0;
        int levelNumber = 0;
        // True only once PHTIFFSlide::extractImages has confirmed the pairing between this
        // level and its tiff directory by an exact declared-size comparison; only a
        // corroborated level may have its tile padding cropped. Defaults to false rather
        // than true: an unverified pairing must not be cropped, so a level has to earn
        // "corroborated" by being matched, not receive it by omission.
        // PHTIFFSlide::extractImages sets the field explicitly on every path it produces,
        // so the default matters only to code that builds a PHTLevel directly instead of
        // through it, e.g. the unit tests.
        bool corroborated = false;

        // Compares only the level/directory pairing, not how sure extractImages was of it:
        // two levels naming the same directory and level number are the same pairing
        // whether or not corroborated agrees, which is what the existing tests assert.
        bool operator==(const PHTLevel& other) const {
            return dirIndex == other.dirIndex && levelNumber == other.levelNumber;
        }
    };

    class SLIDEIO_SVS_EXPORTS PHTIFFSlide : public SVSSlide
    {
    public:
        static std::shared_ptr<SVSSlide> openFile(const std::string& filePath);
    protected:
        PHTIFFSlide() = default;
        void init(const std::vector<TiffDirectory>& directories, TIFFKeeper& keeper) override;
        MetadataBuilder buildMetadataTree() const override;
        void extractImages(const std::vector<TiffDirectory>& directories, const PHTMetadata& metadata,
            std::vector<PHTLevel>& imagePyramid, std::map<std::string, int>& auxImages);
        void createImageScene(const std::vector<TiffDirectory>& directories, const PHTMetadata& metadata,
            const std::vector<PHTLevel>& imagePyramid, libtiff::TIFF* tiff);
        void createAuxScenes(const std::vector<TiffDirectory>& directories,
            const std::map<std::string, int>& auxImages);
    };
}
