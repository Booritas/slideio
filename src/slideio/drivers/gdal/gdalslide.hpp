// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_gdalslide_HPP
#define OPENCV_slideio_gdalslide_HPP

#include "slideio/drivers/gdal/gdal_api_def.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/imagetools/gdal_lib.hpp"
#include <opencv2/core.hpp>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SLIDEIO_GDAL_EXPORTS GDALSlide : public slideio::CVSlide
    {
        friend class GDALImageDriver;
    protected:
        void readMetadata(GDALDatasetH ds);
        GDALSlide(GDALDatasetH ds, const std::string& filePath);
    public:
        virtual ~GDALSlide();
        int getNumScenes() const override;
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
