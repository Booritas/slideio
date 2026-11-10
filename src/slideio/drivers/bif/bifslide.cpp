// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/bif/bifslide.hpp"
#include "slideio/drivers/bif/bifdescription.hpp"
#include "slideio/drivers/bif/bifgeometry.hpp"
#include "slideio/drivers/bif/bifscene.hpp"
#include "slideio/drivers/bif/bifsmallscene.hpp"
#include "slideio/drivers/bif/bifxml.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/log.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include <algorithm>
#include <cmath>
#include <exception>
#include <list>

using namespace slideio;
using namespace slideio::bif;

namespace
{
    struct Level
    {
        TiffDirectory directory;
        LevelDescription description;
    };

    // Design §4.3: levels 0, 1, 2... in chain order, all tiled with one tile size, each
    // an integral downsample of level 0.
    void validateLevels(const std::vector<Level>& levels)
    {
        if (levels.empty()) {
            RAISE_RUNTIME_ERROR << "BIF: the file has no pyramid levels";
        }
        const TiffDirectory& base = levels.front().directory;
        const double mag0 = levels.front().description.magnification;
        for (int index = 0; index < static_cast<int>(levels.size()); ++index) {
            const Level& level = levels[index];
            if (level.description.level != index) {
                RAISE_RUNTIME_ERROR << "BIF: pyramid level " << level.description.level
                    << " found where level " << index << " was expected (IFD "
                    << level.directory.dirIndex << ")";
            }
            if (!level.directory.tiled) {
                RAISE_RUNTIME_ERROR << "BIF: level " << index << " is not tiled";
            }
            if (level.directory.tileWidth != base.tileWidth || level.directory.tileHeight != base.tileHeight) {
                RAISE_RUNTIME_ERROR << "BIF: level " << index << " has tile size "
                    << level.directory.tileWidth << "x" << level.directory.tileHeight
                    << ", level 0 has " << base.tileWidth << "x" << base.tileHeight;
            }
            const double mag = level.description.magnification;
            const double d = mag > 0 ? mag0 / mag : 0.;
            if (!(d >= 1.) || std::abs(d - std::round(d)) > 1.e-6) {
                RAISE_RUNTIME_ERROR << "BIF: level " << index << " has non-integral downsample "
                    << d << " (mag " << mag << " against " << mag0 << " at level 0)";
            }
        }
    }

    // Design §4.5: the AOI's tile grid has to lie on the level-0 storage raster.
    void validateAoi(const Aoi& aoi, const TiffDirectory& base)
    {
        if (aoi.cols < 1 || aoi.rows < 1) {
            RAISE_RUNTIME_ERROR << "BIF: AOI " << aoi.index << " has a " << aoi.cols << "x"
                << aoi.rows << " tile grid";
        }
        if (aoi.tileWidth != base.tileWidth || aoi.tileHeight != base.tileHeight) {
            RAISE_RUNTIME_ERROR << "BIF: AOI " << aoi.index << " has tile size " << aoi.tileWidth
                << "x" << aoi.tileHeight << ", the level-0 TIFF tile is " << base.tileWidth
                << "x" << base.tileHeight;
        }
        if (aoi.originX < 0 || aoi.originY < 0
            || aoi.originX % base.tileWidth != 0 || aoi.originY % base.tileHeight != 0) {
            RAISE_RUNTIME_ERROR << "BIF: AOI " << aoi.index << " origin (" << aoi.originX << ","
                << aoi.originY << ") is not a multiple of the tile size";
        }
        if (static_cast<int64_t>(aoi.originX) + static_cast<int64_t>(aoi.cols) * aoi.tileWidth > base.width
            || static_cast<int64_t>(aoi.originY) + static_cast<int64_t>(aoi.rows) * aoi.tileHeight > base.height) {
            RAISE_RUNTIME_ERROR << "BIF: AOI " << aoi.index << " tile grid runs outside the "
                << base.width << "x" << base.height << " level-0 raster";
        }
    }

    void addAuxImage(const std::string& path, AuxKind kind, const TiffDirectory& dir,
                     std::map<std::string, std::shared_ptr<CVScene>>& images,
                     std::list<std::string>& names)
    {
        const std::string name = auxImageName(kind);
        if (images.count(name) != 0) {
            SLIDEIO_LOG(WARNING) << "BIF: ignoring a second " << name << " image in IFD " << dir.dirIndex;
            return;
        }
        images[name] = std::make_shared<BIFSmallScene>(path, name, dir);
        names.push_back(name);
    }
}

std::shared_ptr<BIFSlide> BIFSlide::openFile(const std::string& path)
{
    SLIDEIO_LOG(INFO) << "BIFSlide::openFile: " << path;
    std::shared_ptr<BIFSlide> slide(new BIFSlide);
    slide->m_filePath = path;
    slide->setDriverId("BIF");
    try {
        slide->init();
    }
    catch (const std::exception& e) {
        // Design §4.5: every refusal names the file as well as the cause.
        throw RuntimeError() << e.what() << " (file: " << path << ")";
    }
    return slide;
}

void BIFSlide::init()
{
    TIFFKeeper tiff(TiffTools::openTiffFile(m_filePath));
    if (!tiff.isValid()) {
        RAISE_RUNTIME_ERROR << "BIF: cannot open the file as TIFF";
    }
    std::vector<TiffDirectory> directories;
    TiffTools::scanFile(tiff.getHandle(), directories);
    if (directories.empty()) {
        RAISE_RUNTIME_ERROR << "BIF: not a Ventana BIF file (no iScan metadata in IFD 0)";
    }
    const std::string acquisitionXml = TiffTools::readXmlPacket(tiff.getHandle(), directories.front());
    const AcquisitionInfo acquisition = parseAcquisitionXml(acquisitionXml);

    std::vector<Level> levels;
    std::vector<std::pair<AuxKind, const TiffDirectory*>> auxDirectories;
    for (const auto& dir : directories) {
        if (auto level = parseLevelDescription(dir.description)) {
            levels.push_back({ dir, *level });
            continue;
        }
        const AuxKind kind = classifyAuxDescription(dir.description);
        if (kind != AuxKind::None) {
            auxDirectories.emplace_back(kind, &dir);
        }
        else {
            SLIDEIO_LOG(INFO) << "BIF: ignoring IFD " << dir.dirIndex << " with description '"
                << dir.description << "'";
        }
    }
    validateLevels(levels);

    int zLayers = acquisition.zLayers;
    for (const auto& level : levels) {
        zLayers = std::max(zLayers, static_cast<int>(TiffTools::readImageDepth(tiff.getHandle(), level.directory)));
    }
    if (zLayers > 1) {
        RAISE_RUNTIME_ERROR << "BIF: multi-layer (Z=" << zLayers << ") files are not supported yet";
    }

    const TiffDirectory& base = levels.front().directory;
    if (base.channels != 3 || base.bitsPerSample != 8) {
        RAISE_RUNTIME_ERROR << "BIF: level 0 has " << base.channels << " channels of "
            << base.bitsPerSample << " bits; only 8-bit RGB is supported";
    }
    std::vector<TiffDirectory> levelDirectories;
    for (const auto& level : levels) {
        levelDirectories.push_back(level.directory);
    }

    // Design §4.4: an absent packet means a plain Ventana TIFF; a present one that holds
    // nothing (all NULs or whitespace) is malformed and must not degrade to the plain path.
    const bool hasStitchPacket = TiffTools::hasXmlPacket(tiff.getHandle(), base);
    const std::string stitchXml = TiffTools::readXmlPacket(tiff.getHandle(), base);
    if (hasStitchPacket && stitchXml.find_first_not_of(" \t\r\n") == std::string::npos) {
        RAISE_RUNTIME_ERROR << "BIF: level-0 stitching packet is empty";
    }
    if (hasStitchPacket) {
        const StitchInfo stitch = parseStitchXml(stitchXml);
        const auto medianX = slideMedianOverlap(stitch.aois, JointAxis::Horizontal);
        const auto medianY = slideMedianOverlap(stitch.aois, JointAxis::Vertical);
        for (const auto& aoi : stitch.aois) {
            if (!aoi.scanned) {
                continue;
            }
            validateAoi(aoi, base);
            AoiGeometry geometry(aoi, estimateOverlaps(aoi, medianX, medianY));
            const int sceneIndex = static_cast<int>(m_scenes.size());
            m_scenes.push_back(std::make_shared<BIFScene>(m_filePath, "AOI" + std::to_string(aoi.index),
                sceneIndex, levelDirectories, std::move(geometry), aoi, acquisition));
        }
        if (m_scenes.empty()) {
            RAISE_RUNTIME_ERROR << "BIF: the slide has no scanned AOI";
        }
    }
    else {
        // A plain Ventana TIFF: one AOI over the whole raster, with tiles that abut.
        Aoi aoi;
        aoi.scanned = true;
        aoi.tileWidth = base.tileWidth;
        aoi.tileHeight = base.tileHeight;
        aoi.cols = (base.width + base.tileWidth - 1) / base.tileWidth;
        aoi.rows = (base.height + base.tileHeight - 1) / base.tileHeight;
        BoundaryOverlaps overlaps;
        overlaps.columns.assign(aoi.cols - 1, 0);
        overlaps.rows.assign(aoi.rows - 1, 0);
        AoiGeometry geometry(aoi, overlaps);
        m_scenes.push_back(std::make_shared<BIFScene>(m_filePath, "Image", 0, levelDirectories,
            std::move(geometry), aoi, acquisition));
    }

    for (const auto& [kind, dir] : auxDirectories) {
        addAuxImage(m_filePath, kind, *dir, m_auxImages, m_auxNames);
    }
    m_rawMetadata = acquisitionXml;
    m_metadataFormat = MetadataFormat::XML;
}

int BIFSlide::getNumScenes() const
{
    return static_cast<int>(m_scenes.size());
}

std::string BIFSlide::getFilePath() const
{
    return m_filePath;
}

std::shared_ptr<CVScene> BIFSlide::getScene(int index) const
{
    if (index < 0 || index >= getNumScenes()) {
        RAISE_RUNTIME_ERROR << "BIF driver: invalid scene index: " << index << ", the slide has "
            << getNumScenes() << " scenes";
    }
    return m_scenes[index];
}

std::shared_ptr<CVScene> BIFSlide::getAuxImage(const std::string& name) const
{
    auto it = m_auxImages.find(name);
    if (it == m_auxImages.end()) {
        RAISE_RUNTIME_ERROR << "BIF: the slide has no auxiliary image " << name;
    }
    return it->second;
}
