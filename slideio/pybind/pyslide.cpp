#include "pyslide.hpp"
#include "pyscene.hpp"

int PySlide::getNumbScenes() const
{
    return m_Slide->getNumbScenes();
}

std::string PySlide::getFilePath() const
{
    return m_Slide->getFilePath();
}

std::shared_ptr<PyScene> PySlide::getScene(int index)
{
    std::shared_ptr<slideio::Scene> scene = m_Slide->getScene(index);
    std::shared_ptr<PyScene> wrapper(new PyScene(std::move(scene)));
    return wrapper;
}
