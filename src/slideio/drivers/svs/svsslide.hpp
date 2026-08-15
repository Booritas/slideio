// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/svs/svs_api_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/imagetools/libtiff.hpp"
#include <map>

#include "slideio/imagetools/tiffkeeper.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SVSSlide;
}


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
        // True only once phExtractImages has confirmed the pairing between this level and
        // its tiff directory by an exact declared-size comparison; only a corroborated
        // level may have its tile padding cropped. Defaults to false rather than true: an
        // unverified pairing must not be cropped, so a level has to earn "corroborated" by
        // being matched, not receive it by omission. phExtractImages sets the field
        // explicitly on every path it produces, so the default matters only to code that
        // builds a PHTLevel directly instead of through it, e.g. the unit tests.
        bool corroborated = false;

        // Compares only the level/directory pairing, not how sure phExtractImages was of
        // it: two levels naming the same directory and level number are the same pairing
        // whether or not corroborated agrees, which is what the existing tests assert.
        bool operator==(const PHTLevel& other) const {
            return dirIndex == other.dirIndex && levelNumber == other.levelNumber;
        }
    };

    class SLIDEIO_SVS_EXPORTS SVSSlide : public slideio::CVSlide
    {
    protected:
        SVSSlide();
    public:
        virtual ~SVSSlide();
        int getNumScenes() const override;
        std::string getFilePath() const override;
        std::shared_ptr<slideio::CVScene> getScene(int index) const override;
        static std::shared_ptr<SVSSlide> openFile(const std::string& path, const std::string& id);
        static void closeFile(libtiff::TIFF* hfile);
        std::shared_ptr<CVScene> getAuxImage(const std::string& sceneName) const override;
        void log();
    protected:
        MetadataBuilder buildMetadataTree() const override;
        // The keeper owns the tiff handle until the scene that reads from it is created:
        // everything before that point can throw, and a handle nobody owns is a handle
        // nobody closes. Ownership is passed on with TIFFKeeper::release at the one place
        // it is handed to a scene.
        void initSVS(const std::vector<TiffDirectory>& directories, TIFFKeeper& keeper);
        static void phExtractImages(const std::vector<TiffDirectory>& directories, std::vector<PHTLevel>& imagePyramid, std::map<std::string, int>& auxImages);
        void phCreateImageScene(const std::vector<TiffDirectory>& directories, const std::vector<PHTLevel>& imagePyramid, libtiff::TIFF* tiff);
        void phCreateAuxScenes(const std::vector<TiffDirectory>& directories, const std::map<std::string, int>& auxImages);
        void initPhTiff(const std::vector<TiffDirectory>& directories, TIFFKeeper& keeper);
    private:
        std::vector<std::shared_ptr<slideio::CVScene>> m_Scenes;
        std::map<std::string, std::shared_ptr<slideio::CVScene>> m_auxImages;
        std::string m_filePath;
    };
}


#if defined(_MSC_VER)
#pragma warning( pop )
#endif
