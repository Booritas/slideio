// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_slide_HPP
#define OPENCV_slideio_slide_HPP

#include "slideio/slideio_def.hpp"
#include "slideio/core/cvscene.hpp"
#include <string>

namespace slideio
{
    class SLIDEIO_EXPORTS CVSlide
    {
    public:
        virtual ~CVSlide(){}
        virtual int getNumScenes() const = 0;
        virtual std::string getFilePath() const = 0;
        virtual std::shared_ptr<CVScene> getScene(int index) const = 0;
    };
}

#endif