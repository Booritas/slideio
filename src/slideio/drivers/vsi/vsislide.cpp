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
using namespace slideio::vsi;


VSISlide::VSISlide(const std::string& filePath) : m_filePath(filePath)
{
    init();
}


void VSISlide::init()
{
    m_vsiFile = std::make_shared<vsi::VSIFile>(m_filePath);
    if(!m_vsiFile->hasMetadata()) {
        // No metadata, treat the vsi file as a normal tiff file
        const int numDirectories = m_vsiFile->getNumTiffDirectories();
        for (int directoryIndex = 0; directoryIndex < numDirectories; ++directoryIndex) {
             auto scene = std::make_shared<VsiFileScene>(m_filePath, m_vsiFile, directoryIndex);
             m_Scenes.push_back(scene);
         }
    } else {
        m_rawMetadata = m_vsiFile->getRawMetadata();
        if (m_vsiFile->getNumEtsFiles() > 0) {
            const int numFiles = m_vsiFile->getNumEtsFiles();
            for (int fileIndex = 0; fileIndex < numFiles; ++fileIndex) {
                auto scene = std::make_shared<EtsFileScene>(m_filePath, m_vsiFile, fileIndex);
                const auto etsFile = m_vsiFile->getEtsFile(fileIndex);
                auto volume = etsFile->getVolume();
                if (volume) {
                    const vsi::StackType stackType = volume->getType();
                    if (stackType == vsi::StackType::DEFAULT_IMAGE || stackType == vsi::StackType::OVERVIEW_IMAGE) {
                        if (stackType == vsi::StackType::DEFAULT_IMAGE) {
                            m_Scenes.push_back(scene);
                        }
                        else if (stackType == vsi::StackType::OVERVIEW_IMAGE) {
                            std::string typeName = vsi::getStackTypeName(stackType);
                            m_auxImages[typeName] = scene;
                            m_auxNames.emplace_back(typeName);
                        }
                        const int auxImages = volume->getNumAuxVolumes();
                        for (int auxIndex = 0; auxIndex < auxImages; ++auxIndex) {
                            auto auxVolume = volume->getAuxVolume(auxIndex);
                            if (auxVolume) {
                                auto auxScene = std::make_shared<VsiFileScene>(m_filePath, m_vsiFile, auxVolume->getIFD());
                                scene->addAuxImage(auxVolume->getName(), auxScene);
                            }
                        }
                    }
                }
            }
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
