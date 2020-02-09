// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_svsimagedriver_HPP
#define OPENCV_slideio_svsimagedriver_HPP

#include "slideio/slideio_def.hpp"
#include "slideio/core/imagedriver.hpp"

namespace cv
{
    namespace slideio
    {
        class SLIDEIO_EXPORTS SVSImageDriver : public cv::slideio::ImageDriver
        {
        public:
            SVSImageDriver();
            ~SVSImageDriver();
            std::string getID() const override;
            cv::Ptr<slideio::Slide> openFile(const std::string& filePath) override;
            std::string getFileSpecs() const override;
        };
    }
}
#endif
