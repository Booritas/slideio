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

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <tinyxml2.h>

#include "slideio/core/tools/tools.hpp"


using namespace slideio;

const char* THUMBNAIL = "Thumbnail";
const char* MACRO = "Macro";
const char* LABEL = "Label";

namespace
{
    // The largest level number the padding can be computed for: 1 << levelNumber has to
    // stay in range.
    const int PH_MAX_LEVEL_NUMBER = 30;

    // A philips zoom level covers the same area as the base level downsampled by
    // 2^levelNumber, rounded up to a whole pixel.
    cv::Size phLevelContentSize(const cv::Size& baseSize, int levelNumber) {
        const int divisor = 1 << levelNumber;
        return {
            (baseSize.width + divisor - 1) / divisor,
            (baseSize.height + divisor - 1) / divisor
        };
    }

    // Philips stores every zoom level padded up to a whole number of tiles, so a level
    // directory is larger than the image it holds: level 8 of Philips-3.tiff is a 512x512
    // directory carrying a 512x392 image. The padding is not image data: left in place it
    // corrupts the scale of the level (44% at level 8 of Philips-4.tiff) and every block
    // read from it. Shrink the directories to the size of their content; the number of
    // tiles is not affected because the padding never exceeds one tile.
    void phCropLevelPadding(const std::vector<slideio::PHTLevel>& imagePyramid,
                            std::vector<slideio::TiffDirectory>& dirs) {
        if (imagePyramid.empty()) {
            return;
        }
        // The levels are sorted by level number and the base level is the reference: it is
        // stored unpadded, as the image size of the philips metadata confirms.
        if (imagePyramid.front().levelNumber != 0) {
            SLIDEIO_LOG(WARNING) << "SVSSlide: philips zoom level 0 is missing."
                " Tile padding of the zoom levels cannot be cropped.";
            return;
        }
        const cv::Size baseSize = {dirs.front().width, dirs.front().height};
        for (size_t index = 0; index < imagePyramid.size(); ++index) {
            const int levelNumber = imagePyramid[index].levelNumber;
            slideio::TiffDirectory& dir = dirs[index];
            if (levelNumber < 0 || levelNumber > PH_MAX_LEVEL_NUMBER) {
                SLIDEIO_LOG(WARNING) << "SVSSlide: unexpected philips zoom level number "
                    << levelNumber << ". Tile padding of the level is not cropped.";
                continue;
            }
            const cv::Size contentSize = phLevelContentSize(baseSize, levelNumber);
            if (contentSize.width > dir.width || contentSize.height > dir.height) {
                SLIDEIO_LOG(WARNING) << "SVSSlide: philips zoom level " << levelNumber
                    << " is expected to be at least " << contentSize.width << "x" << contentSize.height
                    << " but the tiff directory is " << dir.width << "x" << dir.height
                    << ". Tile padding of the level is not cropped.";
                continue;
            }
            dir.width = contentSize.width;
            dir.height = contentSize.height;
        }
    }
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


void SVSSlide::phExtractImages(const std::vector<TiffDirectory>& directories, std::vector<PHTLevel>& imagePyramid,
    std::map<std::string, int>& auxImages) {
	if (directories.empty()) {
		RAISE_RUNTIME_ERROR << "SVSSlide::phExtractImages: empty directory list!";
	}
    const TiffDirectory& dir = directories.front();
    PHTDescription description(dir.description);
    std::vector<tinyxml2::XMLElement*> images = description.getObjectList(description.getRoot(), SCANNED_IMAGE);
    // directory mapping now assumes the TIFF lays out directories exactly as the XML declares them
    int dirIndex = 0;
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
                PHTLevel pyramidLevel;
                pyramidLevel.dirIndex = dirIndex;
                pyramidLevel.levelNumber = description.getAttributeInt(level, LEVEL_NUMBER);
                imagePyramid.push_back(pyramidLevel);
                ++dirIndex;
            }
        } else {
            auxImages.emplace(imageType, dirIndex);
            ++dirIndex;
        }
    }
    std::sort(imagePyramid.begin(), imagePyramid.end(), [](const PHTLevel& a, const PHTLevel& b) {
    	return a.levelNumber < b.levelNumber;
    });
}

void SVSSlide::phCreateImageScene(const std::vector<TiffDirectory>& directories, const std::vector<PHTLevel>& imagePyramid,
    libtiff::TIFF* hFile) {
    std::vector<TiffDirectory> image_dirs;
    image_dirs.reserve(imagePyramid.size());
    for (const auto& level : imagePyramid) {
        image_dirs.push_back(directories[level.dirIndex]);
    }
    phCropLevelPadding(imagePyramid, image_dirs);
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
    std::vector<PHTLevel> imagePyramid;
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
