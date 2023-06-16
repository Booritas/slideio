#include "pyslide.hpp"
#include "pyscene.hpp"

int PySlide::getNumScenes() const
{
    return m_slide->getNumScenes();
}

std::string PySlide::getFilePath() const
{
    return m_slide->getFilePath();
}

std::shared_ptr<PyScene> PySlide::getScene(int index)
{
    std::shared_ptr<slideio::Scene> scene = m_slide->getScene(index);
    std::shared_ptr<PyScene> wrapper(new PyScene(scene, m_slide));
    return wrapper;
}

const std::string& PySlide::getRawMetadata() const
{
    return m_slide->getRawMetadata();
}

std::list<std::string> PySlide::getAuxImageNames() const
{
    return m_slide->getAuxImageNames();
}

int PySlide::getNumAuxImages() const
{
    return m_slide->getNumAuxImages();
}

std::shared_ptr<PyScene> PySlide::getAuxImage(const std::string& imageName)
{
    std::shared_ptr<slideio::Scene> scene = m_slide->getAuxImage(imageName);
    std::shared_ptr<PyScene> wrapper(new PyScene(scene, nullptr));
    return wrapper;
}

