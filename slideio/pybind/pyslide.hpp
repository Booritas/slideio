// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <utility>


#include "pyscene.hpp"
#include "slideio/core/cvslide.hpp"

class PySlide
{
public:
    PySlide(std::shared_ptr<slideio::CVSlide> slide) : m_Slide(std::move(slide))
    {
    }
    int getNumbScenes() const;
    std::string getFilePath() const;
    std::shared_ptr<PyScene> getScene(int index);
private:
    std::shared_ptr<slideio::CVSlide> m_Slide;
};
