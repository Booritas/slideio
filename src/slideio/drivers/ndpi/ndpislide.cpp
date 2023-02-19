// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "slideio/drivers/ndpi/ndpislide.hpp"
#include "slideio/drivers/ndpi/ndpiscene.hpp"
#include "slideio/base/base.hpp"
#include "slideio/drivers/ndpi/ndpitifftools.hpp"
#include "slideio/drivers/ndpi/ndpifile.hpp"

#include <boost/filesystem.hpp>
#include <boost/format.hpp>


using namespace slideio;


NDPISlide::NDPISlide()
{
}

void NDPISlide::constructScenes()
{
    SLIDEIO_LOG(INFO) << "NDPISlide:constructScenes-begin";

    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    int startIndex = 0;
    int endIndex = 0;
    bool keepCount = true;


    for(int index=0; index<directories.size(); ++index)
    {
        SLIDEIO_LOG(INFO) << "NDPISlide processing directory " << index;

        const NDPITiffDirectory& dir = directories[index];
        if(dir.magnification>=0)
        {
            if(keepCount)
            {
                endIndex++;
            }
            else
            {
                RAISE_RUNTIME_ERROR << "NDPIImageDriver: Unexpected TIFF directory structure. File:" << m_filePath;
            }
        }
        else
        {
            keepCount = false;
        }

        if(dir.magnification < -1.5)
        {

            const std::string imageName("map");
            std::shared_ptr<NDPIScene> scene(new NDPIScene);
            scene->init(imageName, m_pfile, index, index + 1);
            m_auxImages[imageName] = scene;
            m_auxNames.push_back(imageName);
        }
        else if (dir.magnification < -0.5)
        {
            const std::string imageName("macro");
            std::shared_ptr<NDPIScene> scene(new NDPIScene);
            scene->init(imageName, m_pfile, index, index + 1);
            m_auxImages[imageName] = scene;
            m_auxNames.push_back(imageName);
        }
    }
    if(endIndex>startIndex)
    {
        std::shared_ptr<NDPIScene> mainScene(new NDPIScene);
        mainScene->init("main", m_pfile, startIndex, endIndex);
        m_Scenes.push_back(mainScene);
    }
    SLIDEIO_LOG(INFO) << "NDPISlide:constructScenes-end";
}

void NDPISlide::init(const std::string& filePath)
{
    SLIDEIO_LOG(INFO) << "NDPIImageDriver:init-begin";
    m_filePath = filePath;
    namespace fs = boost::filesystem;
    if (!fs::exists(m_filePath)) {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: File does not exist:" << m_filePath;
    }
    m_pfile = new NDPIFile;
    m_pfile->init(m_filePath);
    constructScenes();
    SLIDEIO_LOG(INFO) << "NDPIImageDriver:init-end";
}

NDPISlide::~NDPISlide()
{
    if(m_pfile)
    {
        delete m_pfile;
    }
}

int NDPISlide::getNumScenes() const
{
    return (int)m_Scenes.size();
}

std::string NDPISlide::getFilePath() const
{
    return m_filePath;
}

std::shared_ptr<CVScene> NDPISlide::getScene(int index) const
{
    if(index>=getNumScenes())
    {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: invalid m_scene index:" << index << "for file: " << m_filePath;
    }
    return m_Scenes[index];
}

std::shared_ptr<CVScene> NDPISlide::getAuxImage(const std::string& sceneName) const
{
    auto it = m_auxImages.find(sceneName);
    if(it==m_auxImages.end()) {
        RAISE_RUNTIME_ERROR << "NDPIImageDriver: The slide does not have auxiliary image with name:"
                << sceneName << ". File: " << m_filePath;
    }
    return it->second;
}

void NDPISlide::log()
{
    SLIDEIO_LOG(INFO) << "---NDPISlide" << std::endl;
    SLIDEIO_LOG(INFO) << "filePath:" << m_filePath << std::endl;
}
