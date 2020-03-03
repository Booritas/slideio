#include "SlideW.h"

int SlideW::getNumbScenes() const
{
    return m_Slide->getNumbScenes();
}

std::string SlideW::getFilePath() const
{
    return m_Slide->getFilePath();
}

std::shared_ptr<SceneW> SlideW::getScene(int index)
{
    std::shared_ptr<slideio::Scene> scene = m_Slide->getScene(index);
    std::shared_ptr<SceneW> wrapper(new SceneW(std::move(scene)));
    return wrapper;
}
