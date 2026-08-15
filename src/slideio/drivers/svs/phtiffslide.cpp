// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/svs/phtiffslide.hpp"
#include "slideio/drivers/svs/svsdriverids.hpp"
#include "slideio/drivers/svs/svssmallscene.hpp"
#include "slideio/drivers/svs/phtiffscene.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/core/metadata_internal.hpp"
#include "slideio/base/log.hpp"
#include "slideio/drivers/svs/phtdescription.hpp"
#include "slideio/drivers/svs/svstools.hpp"

#include <algorithm>
#include <cctype>
#include <tinyxml2.h>

#include "slideio/core/tools/tools.hpp"


using namespace slideio;
using namespace slideio::phtiff;

// constexpr, not const char*: a const char* is a non-const pointer and so has external
// linkage, and all three driver libraries define these same three symbols.
namespace
{
    constexpr const char* THUMBNAIL = "Thumbnail";
    constexpr const char* MACRO = "Macro";
    constexpr const char* LABEL = "Label";
}

namespace
{
    // The largest level number the padding can be computed for: 1 << levelNumber has to
    // stay in range.
    const int PH_MAX_LEVEL_NUMBER = 30;

    bool phEqualIgnoreCase(const std::string& first, const std::string& second) {
        if (first.size() != second.size()) {
            return false;
        }
        for (size_t index = 0; index < first.size(); ++index) {
            if (std::tolower(static_cast<unsigned char>(first[index]))
                != std::tolower(static_cast<unsigned char>(second[index]))) {
                return false;
            }
        }
        return true;
    }

    // Philips names an auxiliary image in the description of the tiff directory that
    // holds it, sometimes followed by acquisition parameters:
    // "Macro -offset=(0,0)-pixelsize=(0.0315,0.0315)-rois=(...)". Only the leading word
    // names the image. The kinds slideio knows get the name the other drivers use for
    // them, so that a caller does not have to special case the philips spelling.
    std::string phAuxImageName(const std::string& description) {
        const size_t begin = description.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos) {
            return std::string();
        }
        const size_t end = description.find_first_of(" \t\r\n", begin);
        const std::string word = description.substr(begin,
            (end == std::string::npos) ? std::string::npos : end - begin);
        const char* kinds[] = {THUMBNAIL, MACRO, LABEL};
        for (const char* kind : kinds) {
            if (phEqualIgnoreCase(word, kind)) {
                return kind;
            }
        }
        return word;
    }

    // A philips zoom level covers the same area as the base level downsampled by
    // 2^levelNumber, rounded UP to a whole pixel: a level that rounded down would be one
    // column or row short of holding the whole slide. No real file exercises this -- every
    // real base is a multiple of the 512 tile grid, so the division is exact -- which is
    // why phCreateImageScene_roundsAContentSizeUpNotDown exists to pin it.
    cv::Size phLevelContentSize(const cv::Size& baseSize, int levelNumber) {
        const int divisor = 1 << levelNumber;
        return {
            (baseSize.width + divisor - 1) / divisor,
            (baseSize.height + divisor - 1) / divisor
        };
    }

    // Metadata tree helpers. Every philips attribute is optional: the files differ in
    // what their scanner software wrote, and an attribute that is not there simply leaves
    // its key out of the tree. An attribute stored in an unexpected format is reported and
    // skipped rather than raised -- a metadata tree is a view of what the file says, and
    // one odd value must not cost the caller everything else it says.
    void phWarnSkipped(const slideio::PHTDescription::Attribute& attribute, const std::exception& error) {
        SLIDEIO_LOG(WARNING) << "PHTIFF: the philips attribute " << attribute.Name
            << " is left out of the metadata: " << error.what();
    }

    void phSetText(slideio::MetadataBuilder& node, const char* key, slideio::PHTDescription& description,
                   const tinyxml2::XMLElement* element, const slideio::PHTDescription::Attribute& attribute) {
        if (!description.hasAttribute(element, attribute)) {
            return;
        }
        try {
            node[key].set(description.getAttributeText(element, attribute));
        }
        catch (const std::exception& error) {
            phWarnSkipped(attribute, error);
        }
    }

    void phSetInt(slideio::MetadataBuilder& node, const char* key, slideio::PHTDescription& description,
                  const tinyxml2::XMLElement* element, const slideio::PHTDescription::Attribute& attribute) {
        if (!description.hasAttribute(element, attribute)) {
            return;
        }
        try {
            node[key].set(static_cast<int64_t>(description.getAttributeInt(element, attribute)));
        }
        catch (const std::exception& error) {
            phWarnSkipped(attribute, error);
        }
    }

    template <typename Value, typename Read>
    void phSetList(slideio::MetadataBuilder& node, const char* key, slideio::PHTDescription& description,
                   const tinyxml2::XMLElement* element, const slideio::PHTDescription::Attribute& attribute,
                   Read read) {
        if (!description.hasAttribute(element, attribute)) {
            return;
        }
        try {
            const std::vector<Value> values = read(element, attribute);
            if (values.empty()) {
                return;
            }
            slideio::MetadataBuilder array = node[key];
            array.makeArray();
            for (size_t index = 0; index < values.size(); ++index) {
                array[index].set(values[index]);
            }
        }
        catch (const std::exception& error) {
            phWarnSkipped(attribute, error);
        }
    }

    void phSetTextList(slideio::MetadataBuilder& node, const char* key, slideio::PHTDescription& description,
                       const tinyxml2::XMLElement* element, const slideio::PHTDescription::Attribute& attribute) {
        phSetList<std::string>(node, key, description, element, attribute,
            [&description](const tinyxml2::XMLElement* owner, const slideio::PHTDescription::Attribute& attr) {
                return description.getAttributeTextList(owner, attr);
            });
    }

    void phSetDoubleList(slideio::MetadataBuilder& node, const char* key, slideio::PHTDescription& description,
                         const tinyxml2::XMLElement* element, const slideio::PHTDescription::Attribute& attribute) {
        phSetList<double>(node, key, description, element, attribute,
            [&description](const tinyxml2::XMLElement* owner, const slideio::PHTDescription::Attribute& attr) {
                return description.getAttributeDoubleList(owner, attr);
            });
    }

    // True if the element carries at least one of the attributes. Used to decide whether a
    // grouping node (the size, the pixel format, the compression) belongs in the tree at
    // all: MetadataBuilder cannot drop a key once it is created, so the group has to be
    // asked for before it is built.
    bool phHasAny(slideio::PHTDescription& description, const tinyxml2::XMLElement* element,
                  std::initializer_list<const slideio::PHTDescription::Attribute*> attributes) {
        for (const slideio::PHTDescription::Attribute* attribute : attributes) {
            if (description.hasAttribute(element, *attribute)) {
                return true;
            }
        }
        return false;
    }

    void phBuildLevelNode(slideio::MetadataBuilder& node, slideio::PHTDescription& description,
                          const tinyxml2::XMLElement* level) {
        node.makeObject();
        phSetInt(node, "number", description, level, slideio::phtiff::LEVEL_NUMBER);
        phSetInt(node, "columns", description, level, slideio::phtiff::LEVEL_COLUMNS);
        phSetInt(node, "rows", description, level, slideio::phtiff::LEVEL_ROWS);
        phSetDoubleList(node, "pixelSpacing", description, level, slideio::phtiff::IMAGE_RESOLUTION);
        phSetDoubleList(node, "position", description, level, slideio::phtiff::LEVEL_POSITION);
    }

    // One DPScannedImage: the whole slide image or an auxiliary one. The base64 raster of
    // PIM_DP_IMAGE_DATA is deliberately left out -- it is image data, it belongs in a
    // scene, and it makes the description of Philips-2.tiff 844 KB.
    void phBuildImageNode(slideio::MetadataBuilder& node, slideio::PHTDescription& description,
                          const tinyxml2::XMLElement* image) {
        node.makeObject();
        phSetText(node, "type", description, image, slideio::phtiff::IMAGE_TYPE);
        phSetText(node, "sourceFile", description, image, slideio::phtiff::SOURCE_FILE);
        phSetText(node, "derivationDescription", description, image, slideio::phtiff::DERIVATION_DESCRIPTION);
        phSetText(node, "pixelTransformationMethod", description, image, slideio::phtiff::PIXEL_TRANSFORMATION_METHOD);
        if (phHasAny(description, image, {&slideio::phtiff::IMAGE_COLUMNS, &slideio::phtiff::IMAGE_ROWS})) {
            slideio::MetadataBuilder size = node["size"];
            size.makeObject();
            phSetInt(size, "columns", description, image, slideio::phtiff::IMAGE_COLUMNS);
            phSetInt(size, "rows", description, image, slideio::phtiff::IMAGE_ROWS);
        }
        phSetDoubleList(node, "pixelSpacing", description, image, slideio::phtiff::IMAGE_RESOLUTION);
        if (phHasAny(description, image, {
                &slideio::phtiff::SAMPLES_PER_PIXEL, &slideio::phtiff::PHOTOMETRIC_INTERPRETATION,
                &slideio::phtiff::PLANAR_CONFIGURATION, &slideio::phtiff::BITS_ALLOCATED, &slideio::phtiff::BITS_STORED,
                &slideio::phtiff::HIGH_BIT, &slideio::phtiff::PIXEL_REPRESENTATION})) {
            slideio::MetadataBuilder format = node["pixelFormat"];
            format.makeObject();
            phSetInt(format, "samplesPerPixel", description, image, slideio::phtiff::SAMPLES_PER_PIXEL);
            phSetText(format, "photometricInterpretation", description, image, slideio::phtiff::PHOTOMETRIC_INTERPRETATION);
            phSetInt(format, "planarConfiguration", description, image, slideio::phtiff::PLANAR_CONFIGURATION);
            phSetInt(format, "bitsAllocated", description, image, slideio::phtiff::BITS_ALLOCATED);
            phSetInt(format, "bitsStored", description, image, slideio::phtiff::BITS_STORED);
            phSetInt(format, "highBit", description, image, slideio::phtiff::HIGH_BIT);
            phSetInt(format, "pixelRepresentation", description, image, slideio::phtiff::PIXEL_REPRESENTATION);
        }
        if (phHasAny(description, image, {
                &slideio::phtiff::LOSSY_IMAGE_COMPRESSION, &slideio::phtiff::LOSSY_IMAGE_COMPRESSION_METHOD,
                &slideio::phtiff::LOSSY_IMAGE_COMPRESSION_RATIO})) {
            slideio::MetadataBuilder compression = node["compression"];
            compression.makeObject();
            phSetText(compression, "lossy", description, image, slideio::phtiff::LOSSY_IMAGE_COMPRESSION);
            // The method and the ratio are array valued attributes even though philips
            // writes a single entry in each, so they stay arrays here.
            phSetTextList(compression, "method", description, image, slideio::phtiff::LOSSY_IMAGE_COMPRESSION_METHOD);
            phSetDoubleList(compression, "ratio", description, image, slideio::phtiff::LOSSY_IMAGE_COMPRESSION_RATIO);
        }
        // Only the whole slide image has a pyramid; an auxiliary image gets no empty array.
        const std::vector<const tinyxml2::XMLElement*> levels =
            description.getObjectList(image, slideio::phtiff::LEVEL_SEQUENCE, slideio::phtiff::PIXEL_DATA_REPRESENTATION);
        if (!levels.empty()) {
            slideio::MetadataBuilder levelNodes = node["levels"];
            levelNodes.makeArray();
            for (size_t index = 0; index < levels.size(); ++index) {
                // Copy initialization from the sub-builder, not a copy: copying a
                // MetadataBuilder deep copies the subtree into a fresh root, so anything
                // written through a copy is lost. C++17 elides the initialization and the
                // local ends up naming the same node as its parent's storage.
                slideio::MetadataBuilder levelNode = levelNodes[index];
                phBuildLevelNode(levelNode, description, levels[index]);
            }
        }
    }

    // The metadata tree of a philips slide, built from the xml of tiff directory 0. The
    // levels keep the order the metadata declares them in rather than being sorted by
    // level number: the tree reports what the file says.
    slideio::MetadataBuilder phBuildMetadataTree(const std::string& description) {
        slideio::PHTDescription philips(description);
        const tinyxml2::XMLElement* slide = philips.getRoot();
        slideio::MetadataBuilder root;
        root.makeObject();
        phSetText(root, "manufacturer", philips, slide, slideio::phtiff::MANUFACTURER);
        phSetTextList(root, "softwareVersions", philips, slide, slideio::phtiff::SOFTWARE_VERSIONS);
        phSetText(root, "interfaceVersion", philips, slide, slideio::phtiff::UFS_INTERFACE_VERSION);
        phSetText(root, "barcode", philips, slide, slideio::phtiff::UFS_BARCODE);
        const std::vector<const tinyxml2::XMLElement*> images =
            philips.getObjectList(slide, slideio::phtiff::SCANNED_IMAGES, slideio::phtiff::SCANNED_IMAGE);
        if (!images.empty()) {
            slideio::MetadataBuilder imageNodes = root["images"];
            imageNodes.makeArray();
            for (size_t index = 0; index < images.size(); ++index) {
                slideio::MetadataBuilder imageNode = imageNodes[index];
                phBuildImageNode(imageNode, philips, images[index]);
            }
        }
        return root;
    }

    // Philips stores every zoom level padded up to a whole number of tiles, so a level
    // directory is larger than the image it holds: level 8 of Philips-3.tiff is a 512x512
    // directory carrying a 512x392 image. The padding is not image data: left in place it
    // corrupts the scale of the level (44% at level 8 of Philips-4.tiff) and every block
    // read from it. Shrink the directories to the size of their content; Philips pads each
    // level to its own tile grid, so on real files the tile count is unchanged, and the
    // guard below refuses the crop when it would not be.
    void phCropLevelPadding(const std::vector<slideio::PHTLevel>& imagePyramid,
                            std::vector<slideio::TiffDirectory>& dirs) {
        if (imagePyramid.empty()) {
            return;
        }
        // The caller is trusted to pass parallel ranges -- one directory per pyramid
        // level, in the same order. Checking it here costs one comparison and turns a
        // future caller's mistake into a refusal instead of an out-of-bounds read.
        if (dirs.size() != imagePyramid.size()) {
            SLIDEIO_LOG(WARNING) << "PHTIFF: the zoom level list and the directory list"
                " have different lengths (" << imagePyramid.size() << " and " << dirs.size()
                << "). Tile padding is not cropped.";
            return;
        }
        // The levels are sorted by level number and the base level is the reference: it is
        // stored unpadded, as the image size of the philips metadata confirms.
        if (imagePyramid.front().levelNumber != 0) {
            SLIDEIO_LOG(WARNING) << "PHTIFF: philips zoom level 0 is missing."
                " Tile padding of the zoom levels cannot be cropped.";
            return;
        }
        const cv::Size baseSize = {dirs.front().width, dirs.front().height};
        for (size_t index = 0; index < imagePyramid.size(); ++index) {
            const slideio::PHTLevel& level = imagePyramid[index];
            const int levelNumber = level.levelNumber;
            slideio::TiffDirectory& dir = dirs[index];
            if (!level.corroborated) {
                SLIDEIO_LOG(WARNING) << "PHTIFF: philips zoom level " << levelNumber
                    << " was paired with its tiff directory by position, not by a matching"
                    " declared size. Tile padding of the level is not cropped.";
                continue;
            }
            if (levelNumber < 0 || levelNumber > PH_MAX_LEVEL_NUMBER) {
                SLIDEIO_LOG(WARNING) << "PHTIFF: unexpected philips zoom level number "
                    << levelNumber << ". Tile padding of the level is not cropped.";
                continue;
            }
            const cv::Size contentSize = phLevelContentSize(baseSize, levelNumber);
            if (contentSize.width > dir.width || contentSize.height > dir.height) {
                SLIDEIO_LOG(WARNING) << "PHTIFF: philips zoom level " << levelNumber
                    << " is expected to be at least " << contentSize.width << "x" << contentSize.height
                    << " but the tiff directory is " << dir.width << "x" << dir.height
                    << ". Tile padding of the level is not cropped.";
                continue;
            }
            // Shrinking the directory does not change its tile layout, so the crop is only
            // safe while the content occupies the same number of tiles as the stored
            // raster. Philips pads every level to its own tile grid, which makes this hold
            // on every real file; if it ever did not, the tile indices would skew and the
            // level would read the wrong pixels with no error at all.
            if (dir.tileWidth > 0 && dir.tileHeight > 0) {
                const int storedTilesX = (dir.width - 1) / dir.tileWidth + 1;
                const int storedTilesY = (dir.height - 1) / dir.tileHeight + 1;
                const int contentTilesX = (contentSize.width - 1) / dir.tileWidth + 1;
                const int contentTilesY = (contentSize.height - 1) / dir.tileHeight + 1;
                if (storedTilesX != contentTilesX || storedTilesY != contentTilesY) {
                    SLIDEIO_LOG(WARNING) << "PHTIFF: cropping philips zoom level " << levelNumber
                        << " from " << dir.width << "x" << dir.height << " to "
                        << contentSize.width << "x" << contentSize.height
                        << " would change its tile count. The level is not cropped.";
                    continue;
                }
            }
            dir.width = contentSize.width;
            dir.height = contentSize.height;
        }
    }
}

void PHTIFFSlide::init(const std::vector<TiffDirectory>& directories, TIFFKeeper& keeper) {
    if (directories.empty()) {
        RAISE_RUNTIME_ERROR << "PHTIFFSlide: empty directory list!";
    }
    // Philips stores the metadata of the whole slide in the description of tiff
    // directory 0. It is parsed once here and handed to extractImages and
    // createImageScene, rather than each of them (and the scene they build) parsing
    // their own copy of a document that can run to 844 KB.
    const PHTMetadata metadata = readPHTMetadata(directories.front().description);
    std::vector<PHTLevel> imagePyramid;
	std::map<std::string, int> auxImages;
	extractImages(directories, metadata, imagePyramid, auxImages);
	createImageScene(directories, metadata, imagePyramid, keeper.release());
	createAuxScenes(directories, auxImages);
    m_rawMetadata = directories.front().description;
    m_metadataFormat = MetadataFormat::XML;
}

void PHTIFFSlide::extractImages(const std::vector<TiffDirectory>& directories, const PHTMetadata& metadata,
    std::vector<PHTLevel>& imagePyramid, std::map<std::string, int>& auxImages) {
	if (directories.empty()) {
		RAISE_RUNTIME_ERROR << "PHTIFFSlide::extractImages: empty directory list!";
	}
    // Every tiff directory says what it is: philips stores the zoom levels of the pyramid
    // tiled, and the auxiliary images striped and named by their description. The philips
    // metadata must not be used as an index into the file. It declares images the tiff
    // does not store -- Philips-4.tiff declares a label image and stores only the macro --
    // and it declares them in an order of its own, so walking the two side by side hands
    // out the macro raster under the name of the label.
    std::vector<int> levelDirs;
    for (int index = 0; index < static_cast<int>(directories.size()); ++index) {
        const TiffDirectory& dir = directories[index];
        if (dir.tiled) {
            levelDirs.push_back(index);
            continue;
        }
        const std::string name = phAuxImageName(dir.description);
        if (name.empty()) {
            SLIDEIO_LOG(WARNING) << "PHTIFFSlide: the tiff directory " << index << " of the philips file"
                " is neither tiled nor named by its description. The directory is ignored.";
            continue;
        }
        if (!auxImages.emplace(name, index).second) {
            SLIDEIO_LOG(WARNING) << "PHTIFFSlide: the philips file contains more than one auxiliary image '"
                << name << "'. The tiff directory " << index << " is ignored.";
        }
    }
    // Only the philips metadata knows the level number of a zoom level, and the level
    // number is what tells how much of the slide the level covers. The tiff directory
    // size cannot supply it: padding can leave two consecutive levels the same width. A
    // declared level is therefore matched primarily to the tiled directory that names that
    // same level number in its own description; when none does (or the one that does
    // disagrees on size), the fallback matches by declared size instead of position -- a
    // directory the metadata does not account for (an auxiliary image stored tiled, a
    // level it omits) would otherwise shift the level numbers of every directory that
    // follows it, and reproduce the wrong-scale defect the crop below exists to remove.
    std::vector<PHTLevelDeclaration> declaredLevels;
    if (const PHTImageDeclaration* wsi = metadata.wholeSlideImage()) {
        declaredLevels = wsi->levels;
    }

    std::vector<bool> levelClaimed(declaredLevels.size(), false);
    std::vector<bool> dirClaimed(levelDirs.size(), false);

    // Pass one: a level directory names its own level, so pair those first and exactly.
    // Size alone cannot separate two declared levels that share a size, and a directory
    // the metadata never declared can carry that same size -- in which case matching by
    // size hands a real level's place to the interloper and drops the real directory.
    for (size_t dirPos = 0; dirPos < levelDirs.size(); ++dirPos) {
        const TiffDirectory& dir = directories[levelDirs[dirPos]];
        const int named = SVSTools::extractPhilipsLevelNumber(dir.description);
        if (named < 0) {
            continue;
        }
        for (size_t levelIndex = 0; levelIndex < declaredLevels.size(); ++levelIndex) {
            const PHTLevelDeclaration& declared = declaredLevels[levelIndex];
            if (levelClaimed[levelIndex] || declared.number != named) {
                continue;
            }
            // The declared size still has to agree. A directory that names a level but
            // does not match its declared size is a contradiction, and is left to the
            // size pass rather than trusted.
            if (declared.declaredSize.width == dir.width
                && declared.declaredSize.height == dir.height) {
                levelClaimed[levelIndex] = true;
                dirClaimed[dirPos] = true;
                PHTLevel pyramidLevel;
                pyramidLevel.dirIndex = levelDirs[dirPos];
                pyramidLevel.levelNumber = declared.number;
                pyramidLevel.corroborated = true;
                imagePyramid.push_back(pyramidLevel);
            } else {
                SLIDEIO_LOG(WARNING) << "PHTIFFSlide: tiff directory " << levelDirs[dirPos]
                    << " names philips zoom level " << named << " but is " << dir.width << "x"
                    << dir.height << ", not the declared " << declared.declaredSize.width << "x"
                    << declared.declaredSize.height << ". The pairing of the directory to a"
                    " level is left to the size match.";
            }
            break;
        }
    }

    // Pass two: everything still unpaired, by declared size, in file order. This is what
    // places the base level, whose directory carries the xml metadata and names no level.
    for (size_t dirPos = 0; dirPos < levelDirs.size(); ++dirPos) {
        if (dirClaimed[dirPos]) {
            continue;
        }
        const TiffDirectory& dir = directories[levelDirs[dirPos]];
        for (size_t levelIndex = 0; levelIndex < declaredLevels.size(); ++levelIndex) {
            const PHTLevelDeclaration& declared = declaredLevels[levelIndex];
            if (levelClaimed[levelIndex] || declared.declaredSize.width <= 0
                || declared.declaredSize.height <= 0) {
                continue;
            }
            if (declared.declaredSize.width == dir.width && declared.declaredSize.height == dir.height) {
                levelClaimed[levelIndex] = true;
                dirClaimed[dirPos] = true;
                PHTLevel pyramidLevel;
                pyramidLevel.dirIndex = levelDirs[dirPos];
                pyramidLevel.levelNumber = declared.number;
                pyramidLevel.corroborated = true;
                imagePyramid.push_back(pyramidLevel);
                break;
            }
        }
    }

    // A declared level the metadata gives no size for cannot be matched this way: pair it
    // positionally with whatever tiled directory is still unclaimed. The pairing is not
    // verified, so phCropLevelPadding must not crop the level on the strength of it.
    size_t dirCursor = 0;
    for (size_t levelIndex = 0; levelIndex < declaredLevels.size(); ++levelIndex) {
        if (levelClaimed[levelIndex]) {
            continue;
        }
        const PHTLevelDeclaration& declared = declaredLevels[levelIndex];
        if (declared.declaredSize.width > 0 && declared.declaredSize.height > 0) {
            SLIDEIO_LOG(WARNING) << "PHTIFFSlide: the philips zoom level " << declared.number
                << " is declared as " << declared.declaredSize.width << "x" << declared.declaredSize.height
                << " but no tiff directory of that size is left. The level is not used.";
            continue;
        }
        while (dirCursor < levelDirs.size() && dirClaimed[dirCursor]) {
            ++dirCursor;
        }
        if (dirCursor >= levelDirs.size()) {
            SLIDEIO_LOG(WARNING) << "PHTIFFSlide: the philips zoom level " << declared.number
                << " has no declared size and no tiff directory is left to pair it with."
                " The level is not used.";
            continue;
        }
        SLIDEIO_LOG(WARNING) << "PHTIFFSlide: the philips zoom level " << declared.number
            << " has no declared size, so it is paired with tiff directory " << levelDirs[dirCursor]
            << " by position instead. The pairing is not verified and the level's tile"
            " padding is not cropped.";
        dirClaimed[dirCursor] = true;
        PHTLevel pyramidLevel;
        pyramidLevel.dirIndex = levelDirs[dirCursor];
        pyramidLevel.levelNumber = declared.number;
        pyramidLevel.corroborated = false;
        imagePyramid.push_back(pyramidLevel);
        ++dirCursor;
    }

    for (size_t dirPos = 0; dirPos < levelDirs.size(); ++dirPos) {
        if (!dirClaimed[dirPos]) {
            SLIDEIO_LOG(WARNING) << "PHTIFFSlide: the tiff directory " << levelDirs[dirPos]
                << " of the philips file matches no declared zoom level. The directory is ignored.";
        }
    }

    std::sort(imagePyramid.begin(), imagePyramid.end(), [](const PHTLevel& a, const PHTLevel& b) {
        return a.levelNumber < b.levelNumber;
    });
}

void PHTIFFSlide::createImageScene(const std::vector<TiffDirectory>& directories, const PHTMetadata& metadata,
    const std::vector<PHTLevel>& imagePyramid, libtiff::TIFF* hFile) {
    std::vector<TiffDirectory> image_dirs;
    image_dirs.reserve(imagePyramid.size());
    for (const auto& level : imagePyramid) {
        image_dirs.push_back(directories[level.dirIndex]);
    }
    phCropLevelPadding(imagePyramid, image_dirs);
    auto tScene = PHTIFFTiledScene::create(m_filePath, hFile, "Image", image_dirs, metadata);
    tScene->setDriverId(m_driverId);
    std::shared_ptr<CVScene> scene(tScene);
    m_Scenes.push_back(scene);
}

void PHTIFFSlide::createAuxScenes(const std::vector<TiffDirectory>& directories,
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

MetadataBuilder PHTIFFSlide::buildMetadataTree() const
{
    try {
        return phBuildMetadataTree(m_rawMetadata);
    }
    catch (const std::exception& error) {
        // The description of a philips file is the only place the metadata lives, so
        // there is nothing better to fall back on than the generic xml handling, which
        // reports the parse error in the tree instead of raising out of getMetadata().
        SLIDEIO_LOG(WARNING) << "PHTIFF: cannot build the metadata tree of the philips file "
            << m_filePath << ": " << error.what();
        return CVSlide::buildMetadataTree();
    }
}

std::shared_ptr<SVSSlide> PHTIFFSlide::openFile(const std::string& filePath)
{
    return SVSSlide::openFile(filePath, PHTIFF_DRIVER_ID, std::shared_ptr<PHTIFFSlide>(new PHTIFFSlide));
}
