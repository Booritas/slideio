// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_gdalslide_HPP
#define OPENCV_slideio_gdalslide_HPP

#include "slideio/slideio_def.hpp"
#include "slideio/core/CVSlide.hpp"
#include "slideio/core/CVScene.hpp"
#include "slideio/gdal_lib.hpp"
#include <opencv2/core.hpp>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SLIDEIO_EXPORTS GDALSlide : public slideio::CVSlide
    {
    public:
        GDALSlide(GDALDatasetH ds, const std::string& filePath);
        virtual ~GDALSlide();
        int getNumbScenes() const override;
        std::string getFilePath() const override;
        std::shared_ptr<slideio::CVScene> getScene(int index) const override;
    private:
        std::shared_ptr<slideio::CVScene> m_scene;
    };

}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif
