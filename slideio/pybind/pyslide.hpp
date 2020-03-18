// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <utility>


#include "pyscene.hpp"
#include "slideio/slide.hpp"

class PySlide
{
public:
    PySlide(std::shared_ptr<slideio::Slide> slide) : m_Slide(std::move(slide))
    {
    }
    int getNumScenes() const;
    std::string getFilePath() const;
    std::shared_ptr<PyScene> getScene(int index);
    const std::string& getRawMetadata() const;
private:
    std::shared_ptr<slideio::Slide> m_Slide;
};
