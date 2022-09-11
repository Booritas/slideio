// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/ndpi/ndpislide.hpp"
#include "slideio/drivers/ndpi/ndpiscene.hpp"
#include "slideio/core/base.hpp"

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include "slideio/drivers/ndpi/ndpifile.hpp"


using namespace slideio;


NDPISlide::NDPISlide(const std::string& filePath)
{
}

NDPISlide::~NDPISlide()
{
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
    return nullptr;
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
