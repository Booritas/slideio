// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <utility>


#include "SceneW.h"
#include "slideio/core/slide.hpp"

class SlideW
{
public:
    SlideW(std::shared_ptr<slideio::Slide> slide) : m_Slide(std::move(slide))
    {
    }
    int getNumbScenes() const;
    std::string getFilePath() const;
    std::shared_ptr<SceneW> getScene(int index);
private:
    std::shared_ptr<slideio::Slide> m_Slide;
};
