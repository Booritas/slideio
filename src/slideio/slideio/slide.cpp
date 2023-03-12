// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/slideio/slide.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/base/log.hpp"

using namespace slideio;

Slide::Slide(std::shared_ptr<CVSlide> slide) : m_slide(slide)
{
    SLIDEIO_LOG(INFO) << "Slide constructor"; 
}

Slide::~Slide()
{
    SLIDEIO_LOG(INFO) << "Slide destructor"; 
}

int Slide::getNumScenes() const
{
    SLIDEIO_LOG(INFO) << "Slide::getNumScenes"; 
    return m_slide->getNumScenes();
}

std::string Slide::getFilePath() const
{
    SLIDEIO_LOG(INFO) << "Slide::getFilePath"; 
    return m_slide->getFilePath();
}

std::shared_ptr<Scene> Slide::getScene(int index) const
{
    SLIDEIO_LOG(INFO) << "Slide::getScene " << index; 
    std::shared_ptr<CVScene> cvScene = m_slide->getScene(index);
    std::shared_ptr<Scene> scene(new Scene(cvScene));
    return scene;
}

const std::string& Slide::getRawMetadata() const
{
    SLIDEIO_LOG(INFO) << "Slide::getRawMetadata "; 
    return m_slide->getRawMetadata();
}

const std::list<std::string>& Slide::getAuxImageNames() const
{
    SLIDEIO_LOG(INFO) << "Slide::getAuxImageNames "; 
    return m_slide->getAuxImageNames();
}

int Slide::getNumAuxImages() const
{
    SLIDEIO_LOG(INFO) << "Slide::getNumAuxImages "; 
    return m_slide->getNumAuxImages();
}

std::shared_ptr<Scene> Slide::getAuxImage(const std::string& sceneName) const
{
    SLIDEIO_LOG(INFO) << "Slide::getAuxImage " << sceneName; 
    std::shared_ptr<CVScene> cvScene = m_slide->getAuxImage(sceneName);
    std::shared_ptr<Scene> scene(new Scene(cvScene));
    return scene;
}
