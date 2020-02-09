// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_svsslide_HPP
#define OPENCV_slideio_svsslide_HPP

#include "slideio/slideio_def.hpp"
#include "slideio/core/scene.hpp"
#include "slideio/core/slide.hpp"
#include <tiffio.h>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace cv
{
    namespace slideio
    {
        class SLIDEIO_EXPORTS SVSSlide : public slideio::Slide
        {
        protected:
            SVSSlide();
        public:
            virtual ~SVSSlide();
            int getNumbScenes() const override;
            std::string getFilePath() const override;
            cv::Ptr<slideio::Scene> getScene(int index) const override;
            static cv::Ptr<SVSSlide> openFile(const std::string& path);
            static void closeFile(TIFF* hfile);
        private:
            std::vector<cv::Ptr<slideio::Scene>> m_Scenes;
            std::string m_filePath;
        };
    }
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif