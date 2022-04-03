// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_scnslide_HPP
#define OPENCV_slideio_scnslide_HPP

#include "scnscene.hpp"
#include "slideio/slideio_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/imagetools/tifftools.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SLIDEIO_EXPORTS SCNSlide : public slideio::CVSlide
    {
        friend class SCNImageDriver;
    protected:
        SCNSlide(const std::string& filePath);
        void init();
        void constructScenes();
    public:
        virtual ~SCNSlide();
        int getNumScenes() const override;
        std::string getFilePath() const override;
        std::shared_ptr<slideio::CVScene> getScene(int index) const override;
        std::shared_ptr<CVScene> getAuxImage(const std::string& sceneName) const override;
    private:
        std::vector<std::shared_ptr<slideio::SCNScene>> m_Scenes;
        std::map<std::string, std::shared_ptr<slideio::CVScene>> m_auxImages;
        std::string m_filePath;
        TIFFKeeper m_tiff;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif