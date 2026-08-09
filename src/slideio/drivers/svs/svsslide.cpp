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
#include "slideio/drivers/svs/phtdescription.hpp"

#include <filesystem>
#include <fstream>
#include <tinyxml2.h>

#include "slideio/core/tools/tools.hpp"


using namespace slideio;

const char* THUMBNAIL = "Thumbnail";
const char* MACRO = "Macro";
const char* LABEL = "Label";


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

void SVSSlide::initSVS(const std::vector<TiffDirectory>& directories, libtiff::TIFF* hFile) {
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
        std::shared_ptr<SVSTiledScene> tScene(new SVSTiledScene(m_filePath, getDriverId(), hFile,"Image", image_dirs));
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
        std::string fileName = std::filesystem::path(m_filePath).stem().string();
        std::string xmlPath = "D:/Temp/" + fileName + ".xml";
        std::ofstream outFile(xmlPath);
        outFile << m_rawMetadata;
        outFile.close();
#endif
    }
}


void SVSSlide::phExtractImages(const std::vector<TiffDirectory>& directories, std::list<int>& imagePyramid,
    std::map<std::string, int>& auxImages) {
	if (directories.empty()) {
		RAISE_RUNTIME_ERROR << "SVSSlide::phExtractImages: empty directory list!";
	}
    const TiffDirectory& dir = directories.front();
    PHTDescription description(dir.description);
    std::vector<tinyxml2::XMLElement*> images = description.getObjectList(description.getRoot(), SCANNED_IMAGE);
    // directory mapping now assumes the TIFF lays out directories exactly as the XML declares them
    int dirIndex = 0;
    std::list<std::pair<int, int>> imageWidthMap;
    for (const tinyxml2::XMLElement* image :images) {
        if (dirIndex>=directories.size()) {
            break;
        }
        const std::string imageType = description.getAttributeText(image, IMAGE_TYPE);
        if (imageType == WSI) {
			std::vector<tinyxml2::XMLElement*> levels = description.getObjectList(image, PIXEL_DATA_REPRESENTATION);
            for (tinyxml2::XMLElement* level : levels) {
                if (dirIndex>=directories.size()) {
                    break;
                }
                int levelIndex = description.getAttributeInt(level, LEVEL_NUMBER);
                imageWidthMap.emplace_back(dirIndex, levelIndex);
                ++dirIndex;
            }
        } else {
            auxImages.emplace(imageType, dirIndex);
            ++dirIndex;
        }
    }
    imageWidthMap.sort([](const std::pair<int, int>& a, const std::pair<int, int>& b) {
    	return a.second < b.second;
    });
    for (const auto& pair : imageWidthMap) {
        imagePyramid.push_back(pair.first);
    }
}

void SVSSlide::phCreateImageScene(const std::vector<TiffDirectory>& directories, const std::list<int>& imagePyramid,
    libtiff::TIFF* hFile) {
    std::vector<TiffDirectory> image_dirs;
    image_dirs.reserve(imagePyramid.size());
    for (const auto index : imagePyramid) {
        image_dirs.push_back(directories[index]);
    }
    std::shared_ptr<SVSTiledScene> tScene(new SVSTiledScene(m_filePath, getDriverId(), hFile, "Image", image_dirs));
    tScene->setDriverId(m_driverId);
    std::shared_ptr<CVScene> scene(tScene);
    m_Scenes.push_back(scene);
}

void SVSSlide::phCreateAuxScenes(const std::vector<TiffDirectory>& directories,
    const std::map<std::string, int>& auxImages) {
    for (const auto& pair : auxImages) {
        const std::string& name = pair.first;
        int index = pair.second;
        std::shared_ptr<SVSSmallScene> sScene = std::make_shared <SVSSmallScene>(
            m_filePath, getDriverId(), name, directories[index], true);
        sScene->setDriverId(m_driverId);
        std::shared_ptr<CVScene> scene(sScene);
        m_auxImages[name] = scene;
        m_auxNames.emplace_back(name);
    }
}

void SVSSlide::initPhTiff(const std::vector<TiffDirectory>& directories, libtiff::TIFF* hFile) {
    std::list<int> imagePyramid;
	std::map<std::string, int> auxImages;
	phExtractImages(directories, imagePyramid, auxImages);
	phCreateImageScene(directories, imagePyramid, hFile);
	phCreateAuxScenes(directories, auxImages);
}

std::shared_ptr<SVSSlide> SVSSlide::openFile(const std::string& filePath, const std::string& driverId)
{
    SLIDEIO_LOG(INFO) << "SVSSlide::openFile: " << filePath;
    namespace fs = std::filesystem;
    std::shared_ptr<SVSSlide> slide;
    std::vector<TiffDirectory> directories;
    libtiff::TIFF* tiff(nullptr);
    tiff = TiffTools::openTiffFile(filePath);
    if(!tiff) {
        SLIDEIO_LOG(WARNING) << "SVSSlide::openFile: cannot open file " << filePath << " with libtiff";
        return slide;
    }

    TIFFKeeper keeper(tiff);
    TiffTools::scanFile(tiff, directories);

    slide.reset(new SVSSlide);
    slide->setDriverId(driverId);
    slide->m_filePath = filePath;
    if (driverId == "PHTIFF") {
        slide->initPhTiff(directories, keeper.release());
    }
	else {
        slide->initSVS(directories, keeper.release());
    }

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
