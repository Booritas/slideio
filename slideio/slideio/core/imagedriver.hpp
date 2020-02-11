// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_imagedriver_HPP
#define OPENCV_slideio_imagedriver_HPP

#include "slideio/slideio_def.hpp"
#include "slideio/core/slide.hpp"
#include <opencv2/core.hpp>
#include <string>

namespace slideio
{
    class SLIDEIO_EXPORTS ImageDriver
    {
    public:
        virtual ~ImageDriver(){}
        virtual std::string getID() const = 0;
        virtual bool canOpenFile(const std::string& filePath) const;
        virtual std::shared_ptr<Slide> openFile(const std::string& filePath) = 0;
        virtual std::string getFileSpecs() const = 0;
    };
}

#endif
