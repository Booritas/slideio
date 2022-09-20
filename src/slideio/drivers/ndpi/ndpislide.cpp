// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/ndpi/ndpislide.hpp"
#include "slideio/drivers/ndpi/ndpiscene.hpp"
#include "slideio/core/base.hpp"

#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include "ndpitifftools.hpp"
#include "slideio/drivers/ndpi/ndpifile.hpp"


using namespace slideio;


NDPISlide::NDPISlide()
{
}

void NDPISlide::constructScenes()
{
    const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
    int startIndex = 0;
    int endIndex = 0;
    for(int index=0; index<directories.size(); ++index)
    {
        const NDPITiffDirectory& dir = directories[index];
        if(dir.magnification>=0)
        {
            endIndex++;
        }
    }
    if(endIndex>startIndex)
    {
        std::shared_ptr<NDPIScene> mainScene(new NDPIScene);
        mainScene->init("main", m_pfile, startIndex, endIndex);
        m_Scenes.push_back(mainScene);
    }
}

void NDPISlide::init(const std::string& filePath)
{
    m_filePath = filePath;
    namespace fs = boost::filesystem;
    if (!fs::exists(m_filePath)) {
        throw std::runtime_error(std::string("NDPIImageDriver: File does not exist:") + m_filePath);
    }
    m_pfile = new NDPIFile;
    m_pfile->init(m_filePath);
    constructScenes();
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
        throw std::runtime_error("NDPI driver: invalid m_scene index");
    return m_Scenes[index];
}

std::shared_ptr<CVScene> NDPISlide::getAuxImage(const std::string& sceneName) const
{
    auto it = m_auxImages.find(sceneName);
    if(it==m_auxImages.end()) {
        throw std::runtime_error(
            (boost::format("The slide does non have auxiliary image \"%1%\"") % sceneName).str()
        );
    }
    return it->second;
}

void NDPISlide::log()
{
    SLIDEIO_LOG(info) << "---NDPISlide" << std::endl;
    SLIDEIO_LOG(info) << "filePath:" << m_filePath << std::endl;
}
