// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/svs/svsslide.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/drivers/svs/svssmallscene.hpp"
#include "slideio/drivers/svs/svstiledscene.hpp"
#include "slideio/drivers/svs/svstools.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/core/metadata_internal.hpp"
#include "slideio/base/log.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

#include "slideio/core/tools/tools.hpp"


using namespace slideio;

// constexpr, not const char*: a const char* is a non-const pointer and so has external
// linkage, and all three driver libraries define these same three symbols.
namespace
{
    constexpr const char* THUMBNAIL = "Thumbnail";
    constexpr const char* MACRO = "Macro";
    constexpr const char* LABEL = "Label";
}


SVSSlide::SVSSlide()
{
}

SVSSlide::~SVSSlide()
{
}

int SVSSlide::getNumScenes() const
{
    return (int)m_Scenes.size();
}

std::string SVSSlide::getFilePath() const
{
    return m_filePath;
}

std::shared_ptr<CVScene> SVSSlide::getScene(int index) const
{
    if(index>=getNumScenes()) {
        RAISE_RUNTIME_ERROR << "SVS driver: invalid scene index";
    }
    return m_Scenes[index];
}

void SVSSlide::init(const std::vector<TiffDirectory>& directories, TIFFKeeper& keeper) {
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
        // The classification above is done; the scene takes the handle from here.
        auto tScene = SVSTiledScene::create(m_filePath, getDriverId(), keeper.release(), "Image", image_dirs);
        tScene->setDriverId(m_driverId);
        std::shared_ptr<CVScene> scene(tScene);
        scenes.push_back(scene);
    }
    if(thumbnail>=0) {
        std::shared_ptr<SVSSmallScene> sScene(new SVSSmallScene(m_filePath, getDriverId(), THUMBNAIL, directories[thumbnail], true));
        sScene->setDriverId(m_driverId);
        std::shared_ptr<CVScene> scene(sScene);
        auxImages[THUMBNAIL] = scene;
        auxNames.emplace_back(THUMBNAIL);
    }
    if(label>=0) {
        std::shared_ptr<SVSSmallScene> sScene(new SVSSmallScene(m_filePath, getDriverId(), LABEL, directories[label], true));
        sScene->setDriverId(m_driverId);
        std::shared_ptr<CVScene> scene(sScene);
        auxImages[LABEL] = scene;
        auxNames.emplace_back(LABEL);
    }
    if(macro>=0) {
        std::shared_ptr<SVSSmallScene> sScene = std::make_shared <SVSSmallScene>(
            m_filePath, getDriverId(),MACRO, directories[macro], true);
        sScene->setDriverId(m_driverId);
        std::shared_ptr<CVScene> scene(sScene);
        auxImages[MACRO] = scene;
        auxNames.emplace_back(MACRO);
    }
    m_Scenes.assign(scenes.begin(), scenes.end());
    m_auxImages = auxImages;
    m_auxNames = auxNames;

    if(!directories.empty()) {
        const auto& dir = directories.front();
        m_rawMetadata = dir.description;
        m_metadataFormat = MetadataFormat::Text;
#if defined(_DEBUG)
        // std::string fileName = std::filesystem::path(m_filePath).stem().string();
        // std::string xmlPath = "D:/Temp/" + fileName + ".xml";
        // std::ofstream outFile(xmlPath);
        // outFile << m_rawMetadata;
        // outFile.close();
#endif
    }
}


std::shared_ptr<SVSSlide> SVSSlide::openFile(const std::string& filePath, const std::string& driverId)
{
    return openFile(filePath, driverId, std::shared_ptr<SVSSlide>(new SVSSlide));
}

std::shared_ptr<SVSSlide> SVSSlide::openFile(const std::string& filePath, const std::string& driverId,
    std::shared_ptr<SVSSlide> slide)
{
    SLIDEIO_LOG(INFO) << "SVSSlide::openFile: " << filePath;
    namespace fs = std::filesystem;
    std::vector<TiffDirectory> directories;
    libtiff::TIFF* tiff(nullptr);
    tiff = TiffTools::openTiffFile(filePath);
    if(!tiff) {
        SLIDEIO_LOG(WARNING) << "SVSSlide::openFile: cannot open file " << filePath << " with libtiff";
        return std::shared_ptr<SVSSlide>();
    }

    TIFFKeeper keeper(tiff);
    TiffTools::scanFile(tiff, directories);

    slide->setDriverId(driverId);
    slide->m_filePath = filePath;
    slide->init(directories, keeper);

    return slide;
}

std::shared_ptr<CVScene> SVSSlide::getAuxImage(const std::string& sceneName) const
{
    auto it = m_auxImages.find(sceneName);
    if(it==m_auxImages.end()) {
        RAISE_RUNTIME_ERROR << "The slide does not have auxiliary image " << sceneName;
    }
    return it->second;
}

void SVSSlide::log()
{
    SLIDEIO_LOG(INFO) << "---SVSSlide" << std::endl;
    SLIDEIO_LOG(INFO) << "filePath:" << m_filePath << std::endl;
}

MetadataBuilder SVSSlide::buildMetadataTree() const
{
    return detail::builderFromJson(SVSTools::parseAperioMetadata(m_rawMetadata));
}
