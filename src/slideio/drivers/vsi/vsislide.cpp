// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/vsi/vsislide.hpp"
#include <cstdlib>
#include <iomanip>

#include "etsfilescene.hpp"
#include "vsifilescene.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/drivers/vsi/vsifile.hpp"


using namespace slideio;


VSISlide::VSISlide(const std::string& filePath) : m_filePath(filePath)
{
    init();
}


void VSISlide::init()
{
    m_vsiFile = std::make_shared<vsi::VSIFile>(m_filePath);
    m_rawMetadata = m_vsiFile->getRawMetadata();
    const int numDirectories = m_vsiFile->getNumTiffDirectories();
    for (int directoryIndex = 0; directoryIndex < numDirectories; ++directoryIndex) {
        auto scene = std::make_shared<VsiFileScene>(m_filePath, m_vsiFile, directoryIndex);
        m_Scenes.push_back(scene);
    }
    if (m_vsiFile->getNumExternalFiles() > 0) {
        const int numFiles = m_vsiFile->getNumExternalFiles();
        for (int fileIndex = 0; fileIndex < numFiles; ++fileIndex) {
            auto scene = std::make_shared<EtsFileScene>(m_filePath, m_vsiFile, fileIndex);
            m_Scenes.push_back(scene);
        }
    }
}


int VSISlide::getNumScenes() const
{
    return static_cast<int>(m_Scenes.size());
}

std::string VSISlide::getFilePath() const
{
    return m_filePath;
}

std::shared_ptr<CVScene> VSISlide::getScene(int index) const
{
    if (index >= getNumScenes()) {
        RAISE_RUNTIME_ERROR << "VSI driver: invalid m_scene index: " << index << " from " << getNumScenes() <<
            " scenes";
    }
    return m_Scenes[index];
}

std::shared_ptr<CVScene> VSISlide::getAuxImage(const std::string& sceneName) const
{
    auto it = m_auxImages.find(sceneName);
    if (it == m_auxImages.end()) {
        RAISE_RUNTIME_ERROR << "The slide does non have auxiliary image " << sceneName;
    }
    return it->second;
}

const std::string& VSISlide::getRawMetadata() const
{
    return m_rawMetadata;
}
