// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/gdal/gdaltiffscene.hpp"

#include "slideio/core/exceptions.hpp"
#include "slideio/core/levelinfo.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/imagetools/smalltiffwrapper.hpp"

#include <opencv2/imgproc.hpp>
#include <algorithm>

using namespace slideio;

namespace
{
    // What Tiler's methods receive as userData for one level read: the directory
    // being read (immutable, shared between threads) and the context borrowed for
    // that one read -- the only place a tiff handle enters the read path. Acquired
    // once by readLevelRect and never re-acquired below it.
    struct GDALTileComposerUserData
    {
        const TiffDirectory* dir = nullptr;
        GDALReadContext* context = nullptr;
    };

    // A directory read tile by tile. Anything else -- a striped directory, or one
    // claiming tiles of no size -- is decoded whole for every block.
    bool isTiledLevel(const TiffDirectory& dir)
    {
        return dir.tiled && dir.tileWidth > 0 && dir.tileHeight > 0;
    }

    // The tile's rect in level pixels, clipped to the level at the right and bottom.
    cv::Rect tileRectOf(const TiffDirectory& dir, int tileIndex)
    {
        const int tilesX = (dir.width - 1) / dir.tileWidth + 1;
        const int x = (tileIndex % tilesX) * dir.tileWidth;
        const int y = (tileIndex / tilesX) * dir.tileHeight;
        return {x, y, std::min(dir.tileWidth, dir.width - x), std::min(dir.tileHeight, dir.height - y)};
    }

    const GDALTileComposerUserData& userDataOf(void* userData)
    {
        return *static_cast<const GDALTileComposerUserData*>(userData);
    }
}

GDALReadContext::GDALReadContext(const std::string& filePath)
    : keeper(TiffTools::openTiffFile(filePath))
{
    if (!keeper.isValid()) {
        RAISE_RUNTIME_ERROR << "GDALDriver: cannot open file " << filePath;
    }
}

// The factory captures the path by value, not `this`, as SVSTiledScene's does.
GDALTiffScene::GDALTiffScene(const std::string& filePath, const std::string& driverId,
                             std::vector<TiffDirectory> levels)
    : m_filePath(filePath),
      m_driverId(driverId),
      m_directories(std::move(levels)),
      m_contextPool([filePath]() { return std::make_unique<GDALReadContext>(filePath); })
{
    if (m_directories.empty()) {
        RAISE_RUNTIME_ERROR << "GDALDriver: a tiff scene needs at least one directory. File: " << filePath;
    }
    const TiffDirectory& base = m_directories.front();
    // The same string SmallTiffPage reported for this directory, so level 0 of a
    // file that opened before pyramids keeps its metadata.
    m_metadata = SmallTiffWrapper::directoryMetadata(base);
    if (const auto acquired = Tools::parseTiffDateTime(base.dateTime)) {
        m_acquisitionTime = *acquired;
    }
    // Bytes only: the driver never parses the profile, it surfaces what the scan found.
    m_colorProfile = ColorProfile(base.iccProfile);

    const int numLevels = static_cast<int>(m_directories.size());
    m_levels.resize(numLevels);
    for (int index = 0; index < numLevels; ++index) {
        const TiffDirectory& dir = m_directories[index];
        // A tiled level reports the file's own tiles; anything else has to be
        // decoded whole whatever is asked of it, and says so by being its own tile.
        const cv::Size tileSize = isTiledLevel(dir) ? cv::Size(dir.tileWidth, dir.tileHeight)
                                                    : cv::Size(dir.width, dir.height);
        LevelInfo& level = m_levels[index];
        level.setLevel(index);
        level.setScale(static_cast<double>(dir.width) / static_cast<double>(base.width));
        level.setMagnification(0.);
        level.setSize({dir.width, dir.height});
        level.setTileSize({tileSize.width, tileSize.height});
    }
}

int GDALTiffScene::getNumChannels() const
{
    return m_directories.front().channels;
}

DataType GDALTiffScene::getChannelDataType(int /*channel*/) const
{
    return m_directories.front().dataType;
}

Resolution GDALTiffScene::getResolution() const
{
    return m_directories.front().res;
}

cv::Rect GDALTiffScene::getRect() const
{
    const TiffDirectory& base = m_directories.front();
    return {0, 0, base.width, base.height};
}

Compression GDALTiffScene::getCompression() const
{
    return m_directories.front().slideioCompression;
}

int GDALTiffScene::findZoomLevelIndex(double zoom) const
{
    const double baseWidth = static_cast<double>(m_directories.front().width);
    const auto& directories = m_directories;
    return Tools::findZoomLevel(zoom, static_cast<int>(directories.size()),
        [&directories, baseWidth](int index) { return directories[index].width / baseWidth; });
}

void GDALTiffScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& channelIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
    if (zSliceIndex != 0 || tFrameIndex != 0) {
        RAISE_RUNTIME_ERROR << "GDALDriver: Z-slice and T-frame indices are not supported";
    }
    const auto channels = Tools::completeChannelList(channelIndices, getNumChannels());
    readClampedBlock(getRect(), blockRect, blockSize, channelIndices,
        [&](const cv::Rect& rect, const cv::Size& size, cv::OutputArray block) {
            // Level selection as SVSTiledScene does it: the smallest level that still
            // serves the zoom, and the rect scaled into that level's pixels.
            const double zoomX = static_cast<double>(size.width) / static_cast<double>(rect.width);
            const double zoomY = static_cast<double>(size.height) / static_cast<double>(rect.height);
            const int level = findZoomLevelIndex(std::max(zoomX, zoomY));
            const TiffDirectory& base = m_directories.front();
            const TiffDirectory& dir = m_directories[level];
            cv::Rect levelRect;
            Tools::scaleRect(rect, static_cast<double>(dir.width) / static_cast<double>(base.width),
                             static_cast<double>(dir.height) / static_cast<double>(base.height), levelRect);
            // A rect inside the base scales to one inside the level; the clip only
            // guards against rounding.
            levelRect &= cv::Rect(0, 0, dir.width, dir.height);
            readLevelRect(level, levelRect, size, channels, block);
        },
        output);
}

void GDALTiffScene::readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
    const cv::Size& blockSize, const std::vector<int>& channelIndices,
    int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
    if (zSliceIndex != 0 || tFrameIndex != 0) {
        RAISE_RUNTIME_ERROR << "GDALDriver: Z-slice and T-frame indices are not supported";
    }
    validateLevel(level);
    const auto channels = Tools::completeChannelList(channelIndices, getNumChannels());
    const TiffDirectory& dir = m_directories[level];
    readClampedBlock(cv::Rect(0, 0, dir.width, dir.height), levelRect, blockSize, channelIndices,
        [&](const cv::Rect& rect, const cv::Size& size, cv::OutputArray block) {
            readLevelRect(level, rect, size, channels, block);
        },
        output);
}

void GDALTiffScene::readLevelRect(int level, const cv::Rect& levelRect, const cv::Size& blockSize,
                                  const std::vector<int>& channelIndices, cv::OutputArray output)
{
    const TiffDirectory& dir = m_directories[level];
    // One borrow for the whole read: readTile below reads the handle out of this
    // same context and never acquires one of its own.
    auto borrow = m_contextPool.acquire();
    GDALReadContext& context = borrow.as<GDALReadContext>();
    if (isTiledLevel(dir)) {
        GDALTileComposerUserData userData{&dir, &context};
        TileComposer::composeRect(this, channelIndices, levelRect, blockSize, output, &userData);
        return;
    }
    // The whole level, then cropped and resized: what GDALScene::readWholePageBlock
    // did for a striped page through SmallTiffPage::readRaster. See TECH_DEBT on
    // striped levels.
    cv::Mat raster;
    if (dir.tiled) {
        TiffTools::readTiledDir(context.keeper.getHandle(), dir, raster);
    } else {
        TiffTools::readStripedDir(context.keeper.getHandle(), dir, raster);
    }
    cv::Mat levelRaster;
    if (Tools::isConsecutiveFromZero(channelIndices, dir.channels)) {
        levelRaster = raster;
    } else {
        std::vector<cv::Mat> channels(channelIndices.size());
        int channelNum = 0;
        for (const auto& channelIndex : channelIndices) {
            cv::extractChannel(raster, channels[channelNum++], channelIndex);
        }
        cv::merge(channels, levelRaster);
    }
    cv::Mat blockRaster = levelRaster(levelRect);
    if (blockSize.width != levelRect.width || blockSize.height != levelRect.height) {
        cv::resize(blockRaster, blockRaster, blockSize, 0, 0, cv::INTER_LINEAR);
    }
    blockRaster.copyTo(output);
}

int GDALTiffScene::getTileCount(void* userData)
{
    const TiffDirectory& dir = *userDataOf(userData).dir;
    const int tilesX = (dir.width - 1) / dir.tileWidth + 1;
    const int tilesY = (dir.height - 1) / dir.tileHeight + 1;
    return tilesX * tilesY;
}

bool GDALTiffScene::getTileRect(int tileIndex, cv::Rect& tileRect, void* userData)
{
    // Tiles are numbered row by row, as libtiff numbers a single-plane directory.
    // Edge tiles are clipped to the level, and readTile crops them to match.
    // Clipping only the rect is not enough: TileComposer resizes the whole tile
    // before it intersects it with the block, so for a scaled read the padding an
    // encoder writes past the level edge would be interpolated into the last real
    // pixels.
    tileRect = tileRectOf(*userDataOf(userData).dir, tileIndex);
    return true;
}

bool GDALTiffScene::readTile(int tileIndex, const std::vector<int>& channelIndices,
                             cv::OutputArray tileRaster, void* userData)
{
    // A tile that fails to decode fails the read: the whole-page path never
    // painted over a bad tile either, so there is no zero-fill here, unlike SVS.
    // For an interleaved directory every channel in file order is passed as the
    // empty list, which copies the decoded tile as it is; an explicit list costs a
    // split and a merge per tile. A planar directory reads one plane per listed
    // channel, so the empty list would read none: it always gets the explicit list.
    const GDALTileComposerUserData& data = userDataOf(userData);
    const TiffDirectory& dir = *data.dir;
    static const std::vector<int> allChannels;
    const bool wholeTile = dir.interleaved && Tools::isConsecutiveFromZero(channelIndices, dir.channels);
    cv::Mat decoded;
    TiffTools::readTile(data.context->keeper.getHandle(), dir, tileIndex,
                        wholeTile ? allChannels : channelIndices, decoded);

    // TileComposer resizes whatever comes back to the tile rect, so a tile of the
    // wrong size or type would be stretched or silently dropped rather than fail.
    // A decoder may return the padded tile or, for JPEG 2000, just the valid part;
    // either is cropped to the valid part. Anything smaller is an error.
    const cv::Rect validRect = tileRectOf(dir, tileIndex);
    const int expectedType = CV_MAKETYPE(CVTools::toOpencvType(dir.dataType),
                                         static_cast<int>(channelIndices.size()));
    if (decoded.cols < validRect.width || decoded.rows < validRect.height || decoded.type() != expectedType) {
        RAISE_RUNTIME_ERROR << "GDALDriver: tile " << tileIndex << " of directory " << dir.dirIndex
            << " decodes to " << decoded.cols << "x" << decoded.rows << " type " << decoded.type()
            << ", expected at least " << validRect.width << "x" << validRect.height
            << " type " << expectedType;
    }
    decoded(cv::Rect(0, 0, validRect.width, validRect.height)).copyTo(tileRaster);
    return true;
}

void GDALTiffScene::initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices,
                                    cv::OutputArray output)
{
    const int numChannels = static_cast<int>(channelIndices.size());
    output.create(blockSize, CV_MAKETYPE(CVTools::toOpencvType(m_directories.front().dataType), numChannels));
    output.setTo(0);
}
