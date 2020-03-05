// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/slide.hpp"
#include "slideio/core/cvslide.hpp"

using namespace slideio;

Slide::Slide(std::shared_ptr<CVSlide> slide) : m_slide(slide)
{
}

Slide::~Slide()
{
}

int Slide::getNumbScenes() const
{
    return m_slide->getNumbScenes();
}

std::string Slide::getFilePath() const
{
    return m_slide->getFilePath();
}

std::shared_ptr<Scene> Slide::getScene(int index) const
{
    std::shared_ptr<CVScene> cvScene = m_slide->getScene(index);
    std::shared_ptr<Scene> scene(new Scene(cvScene));
    return scene;
}
