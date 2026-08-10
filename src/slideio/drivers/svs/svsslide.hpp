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
        void initSVS(const std::vector<TiffDirectory>& directories, libtiff::TIFF* hFile);
        static void phExtractImages(const std::vector<TiffDirectory>& directories, std::vector<PHTLevel>& imagePyramid, std::map<std::string, int>& auxImages);
        void phCreateImageScene(const std::vector<TiffDirectory>& directories, const std::vector<PHTLevel>& imagePyramid, libtiff::TIFF* tiff);
        void phCreateAuxScenes(const std::vector<TiffDirectory>& directories, const std::map<std::string, int>& auxImages);
        void initPhTiff(const std::vector<TiffDirectory>& directories, libtiff::TIFF* hFile);
    private:
        std::vector<std::shared_ptr<slideio::CVScene>> m_Scenes;
        std::map<std::string, std::shared_ptr<slideio::CVScene>> m_auxImages;
        std::string m_filePath;
    };
}


#if defined(_MSC_VER)
#pragma warning( pop )
#endif
