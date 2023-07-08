// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_VSIslide_HPP
#define OPENCV_slideio_VSIslide_HPP

#include "vsiscene.hpp"
#include "slideio/drivers/vsi/vsi_api_def.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/imagetools/tifftools.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SLIDEIO_VSI_EXPORTS VSISlide : public slideio::CVSlide
    {
        friend class VSIImageDriver;
    protected:
        VSISlide(const std::string& filePath);
        void init();
    public:
        virtual ~VSISlide();
        int getNumScenes() const override;
        std::string getFilePath() const override;
        std::shared_ptr<slideio::CVScene> getScene(int index) const override;
        std::shared_ptr<CVScene> getAuxImage(const std::string& sceneName) const override;
    private:
        std::vector<std::shared_ptr<slideio::VSIScene>> m_Scenes;
        std::map<std::string, std::shared_ptr<slideio::CVScene>> m_auxImages;
        std::string m_filePath;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif