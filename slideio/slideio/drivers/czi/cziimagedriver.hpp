// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_cziimagedriver_HPP
#define OPENCV_slideio_cziimagedriver_HPP

#include "slideio/slideio_def.hpp"
#include "slideio/core/imagedriver.hpp"
#include "slideio/core/slide.hpp"
#include <opencv2/core.hpp>
#include <string>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace cv
{
    namespace slideio
    {
        class SLIDEIO_EXPORTS CZIImageDriver : public slideio::ImageDriver
        {
        public:
            CZIImageDriver();
            ~CZIImageDriver();
            std::string getID() const override;
            cv::Ptr<Slide> openFile(const std::string& filePath) override;
            std::string getFileSpecs() const override;
        private:
            static std::string filePathPattern;
        };
    }
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif
