// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/pke/pkeslide.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/drivers/pke/pkesmallscene.hpp"
#include "slideio/drivers/pke/pketiledscene.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/base/base.hpp"

#include <boost/filesystem.hpp>
#include <boost/format.hpp>


using namespace slideio;

const char* THUMBNAIL = "Thumbnail";
const char* MACRO = "Macro";
const char* LABEL = "Label";


PKESlide::PKESlide()
{
}

PKESlide::~PKESlide()
{
}

int PKESlide::getNumScenes() const
{
    return (int)m_Scenes.size();
}

std::string PKESlide::getFilePath() const
{
    return m_filePath;
}

std::shared_ptr<CVScene> PKESlide::getScene(int index) const
{
    if(index>=getNumScenes())
        throw std::runtime_error("PKE driver: invalid m_scene index");
    return m_Scenes[index];
}

std::shared_ptr<PKESlide> PKESlide::openFile(const std::string& filePath)
{
    SLIDEIO_LOG(INFO) << "PKESlide::openFile: " << filePath;
    namespace fs = boost::filesystem;
    std::shared_ptr<PKESlide> slide;
    std::vector<TiffDirectory> directories;
    libtiff::TIFF* tiff(nullptr);
    tiff = TiffTools::openTiffFile(filePath);
    if(!tiff) {
        SLIDEIO_LOG(WARNING) << "PKESlide::openFile: cannot open file " << filePath << " with libtiff";
        return slide;
    }
    TIFFKeeper keeper(tiff);

    TiffTools::scanFile(tiff, directories);

    std::vector<int> image;
    int thumbnail(-1), macro(-1), label(-1);
    image.push_back(0);
    int nextDir = 1;
    if(static_cast<int>(directories.size()) > nextDir) {
        if(!directories[nextDir].tiled){
            thumbnail = nextDir;
            nextDir++;
        }
    }
    for(int dir=nextDir; dir<directories.size(); dir++) {
        auto directory = directories[dir];
        if(!directory.tiled)
            break;
        image.push_back(dir);
        nextDir++;
    }
    for(;nextDir<directories.size(); nextDir++) {
        auto directory = directories[nextDir];
        if(directory.description.find("label")!=std::string::npos)
            label = nextDir;
        else if(directory.description.find("macro")!=std::string::npos)
            macro = nextDir;
    }
    std::vector<std::shared_ptr<CVScene>> scenes;
    std::map<std::string, std::shared_ptr<CVScene>> auxImages;
    std::list<std::string> auxNames;

    if(!image.empty()){
        std::vector<TiffDirectory> image_dirs;
        image_dirs.reserve(image.size());
        for(const auto index: image){
            image_dirs.push_back(directories[index]);
        }
        std::shared_ptr<CVScene> scene(new PKETiledScene(filePath,keeper.release(),"Image", image_dirs));
        scenes.push_back(scene);
    }
    if(thumbnail>=0) {
        std::shared_ptr<CVScene> scene(new PKESmallScene(filePath, THUMBNAIL, directories[thumbnail], true));
        auxImages[THUMBNAIL] = scene;
        auxNames.emplace_back(THUMBNAIL);
    }
    if(label>=0) {
        std::shared_ptr<CVScene> scene(new PKESmallScene(filePath,LABEL, directories[label], true));
        auxImages[LABEL] = scene;
        auxNames.emplace_back(LABEL);
    }
    if(macro>=0) {
        std::shared_ptr<CVScene> scene = std::make_shared <PKESmallScene>(
            filePath, MACRO, directories[macro], tiff);
        auxImages[MACRO] = scene;
        auxNames.emplace_back(MACRO);
    }
    slide.reset(new PKESlide);
    slide->m_Scenes.assign(scenes.begin(), scenes.end());
    slide->m_filePath = filePath;
    slide->m_auxImages = auxImages;
    slide->m_auxNames = auxNames;

    if(!directories.empty()) {
        const auto& dir = directories.front();
        slide->m_rawMetadata = dir.description;
    }
    return slide;
}

std::shared_ptr<CVScene> PKESlide::getAuxImage(const std::string& sceneName) const
{
    auto it = m_auxImages.find(sceneName);
    if(it==m_auxImages.end()) {
        throw std::runtime_error(
            (boost::format("The slide does non have auxiliary image \"%1%\"") % sceneName).str()
        );
    }
    return it->second;
}

void PKESlide::log()
{
    SLIDEIO_LOG(INFO) << "---PKESlide" << std::endl;
    SLIDEIO_LOG(INFO) << "filePath:" << m_filePath << std::endl;
}
