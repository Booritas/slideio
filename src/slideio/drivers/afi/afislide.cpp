// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/base/exceptions.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/drivers/afi/afislide.hpp"
#include "slideio/drivers/svs/svsslide.hpp"
#include <filesystem>
#include <tinyxml2.h>
#include <fstream>


using namespace slideio;
using namespace tinyxml2;

namespace {

static std::vector<const XMLElement*> getXmlElementsByPath(const XMLNode* parent, std::string path)
{
    const auto delim = path.find('/');
    const auto tag = path.substr(0, delim);
    const XMLElement* xmlCurrentNode = parent->FirstChildElement(tag.c_str());
    std::vector<const XMLElement*> result;
    while (xmlCurrentNode) {
        if (tag == path) {
            result.push_back(xmlCurrentNode);
        }
        else {
            const auto elems = getXmlElementsByPath(xmlCurrentNode, path.substr(delim + 1));
            result.insert(result.end(), elems.begin(), elems.end());
        }
        xmlCurrentNode = xmlCurrentNode->NextSiblingElement(tag.c_str());
    }
    return result;
}

static std::string getFileRelativeTo(std::string rootFile, std::string relativeFile)
{
    std::filesystem::path p(relativeFile);
    if (p.is_absolute()) {
        return relativeFile;
    }
    else {
        auto retPath = std::filesystem::path(rootFile).parent_path() / p.filename();
        return retPath.generic_string();
    }
}

}

AFISlide::AFISlide()
{
}

AFISlide::~AFISlide()
{
}

int AFISlide::getNumScenes() const
{
    return (int)m_scenes.size();
}

std::string AFISlide::getFilePath() const
{
    return m_filePath;
}

std::shared_ptr<CVScene> AFISlide::getScene(int index) const
{
    if(index>=getNumScenes()) {
        RAISE_RUNTIME_ERROR << "AFI driver: invalid scene index: " 
            << index << ". Total scenes: " << getNumScenes();
    }
    return m_scenes[index];
}

std::shared_ptr<AFISlide> AFISlide::openFile(const std::string& filePath)
{
    Tools::throwIfPathNotExist(filePath, "AFISlide::openFile");
#if defined(WIN32)
    std::wstring filePathW = Tools::toWstring(filePath);
    std::ifstream ifs(filePathW);
#else
    std::ifstream ifs(filePath);
#endif
    if(!ifs.good()) {
        RAISE_RUNTIME_ERROR << "File doesn't exist " << filePath;
    }
    std::string fileString((std::istreambuf_iterator<char>(ifs)),
                            std::istreambuf_iterator<char>());
    const auto files = getFileList(fileString);
    const auto slidesScenes = getSlidesScenesFromFiles(files, filePath);
    if(slidesScenes.first.empty()) {
        RAISE_RUNTIME_ERROR << "File " << filePath << " contains no images to open";
    }   
    std::shared_ptr<AFISlide> afiSlide(new AFISlide);
    afiSlide->m_scenes.assign(slidesScenes.second.begin(), slidesScenes.second.end());
    afiSlide->m_slides.assign(slidesScenes.first.begin(), slidesScenes.first.end());
    afiSlide->m_filePath = filePath;

    return afiSlide;
}

std::vector<std::string> slideio::AFISlide::getFileList(std::string xmlString)
{
    XMLDocument doc;
    const XMLError error = doc.Parse(xmlString.c_str(), xmlString.length());
    if(error != XML_SUCCESS) {
        RAISE_RUNTIME_ERROR << "AFIImageDriver: Error parsing metadata xml";
    }
    std::vector<std::string> result;
    const auto elems = getXmlElementsByPath(&doc, "ImageList/Image/Path");
    std::transform(elems.begin(), elems.end(), std::back_inserter(result), [](auto node) {
        return node->GetText();
        });
    return result;
}

slideio::AFISlide::SlidesScenes slideio::AFISlide::getSlidesScenesFromFiles(const std::vector<std::string>& files,
                                                                            std::string mainFile)
{
    SlidesScenes result;
    for (const auto svsFile : files) {
        const auto svsPath = getFileRelativeTo(mainFile, svsFile);
        const auto svsSlide = SVSSlide::openFile(svsPath);
        if(svsSlide == nullptr) {
            RAISE_RUNTIME_ERROR << "Couldn't open SVS file " << svsPath;
        }
        const auto scenesNum = result.second.size();
        for (decltype (svsSlide->getNumScenes()) i = 0; i < svsSlide->getNumScenes(); ++i) {
            if (svsSlide->getScene(i)->getName() == "Image") {
                result.second.push_back(svsSlide->getScene(i));
            }
        }
        // If some scenes added from that slide
        if (result.second.size() > scenesNum) {
            result.first.push_back(svsSlide);
        }
        else {
            RAISE_RUNTIME_ERROR << "Slide " << svsPath << " didn't have any scene";
        }
    }

    return result;
}

