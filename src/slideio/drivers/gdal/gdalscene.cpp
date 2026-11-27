// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/exceptions.hpp"
#include "slideio/drivers/gdal/gdalscene.hpp"

#include <opencv2/imgproc.hpp>

#include "slideio/slideio/slideio.hpp"
#include "slideio/core/resolution.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/core/levelinfo.hpp"
#include "slideio/imagetools/smallimage.hpp"
#include "slideio/imagetools/smalltiffwrapper.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/core/tools/cvtools.hpp"

#include <algorithm>
#include <numeric>


slideio::GDALScene::GDALScene(const std::shared_ptr<SmallImage>& image, int pageIndex, const std::string& path,
                              const std::string& driverId, std::shared_ptr<std::mutex> readMutex) :
    m_image(image),
    m_imagePage(image->readPage(pageIndex)),
    m_filePath(path),
	m_driverId(driverId),
    m_readMutex(readMutex ? std::move(readMutex) : std::make_shared<std::mutex>())
{
    m_filePath = path;
    // A gdal image has no pyramid, but a scene with no level cannot be addressed by level
    // at all, so it registers the single level it is. Same shape as SVSSmallScene.
    if (m_imagePage != nullptr) {
        if (const auto acquired = Tools::parseTiffDateTime(m_imagePage->getAcquisitionDateTime())) {
            m_acquisitionTime = *acquired;
        }
        const cv::Size imageSize = m_imagePage->getSize();
        // A tiled tiff page is read tile by tile, so it reports the file's own tiles.
        // Anything else has to be decoded whole whatever is asked of it, and says so
        // by reporting the page as its one tile.
        cv::Size tileSize = imageSize;
        if (auto* tiff = dynamic_cast<SmallTiffWrapper*>(m_image.get())) {
            const TiffDirectory& directory = tiff->getDirectory(pageIndex);
            if (directory.tiled && directory.tileWidth > 0 && directory.tileHeight > 0) {
                m_tiff = tiff;
                m_tiledDirectory = &directory;
                tileSize = {directory.tileWidth, directory.tileHeight};
            }
        }
        LevelInfo level;
        level.setLevel(0);
        level.setScale(1.);
        level.setMagnification(getMagnification());
        level.setSize({imageSize.width, imageSize.height});
        level.setTileSize({tileSize.width, tileSize.height});
        m_levels.push_back(level);
        // Bytes only here: the driver never parses the profile it finds, it
        // just surfaces whatever the underlying reader (FreeImage or the TIFF
        // scan) already extracted for this page.
        m_colorProfile = ColorProfile(m_imagePage->getICCProfile());
    }
}


std::string slideio::GDALScene::getFilePath() const
{
    return m_filePath;
}


int slideio::GDALScene::getNumChannels() const
{
    if (m_imagePage == nullptr) {
        RAISE_RUNTIME_ERROR << "GDALDriver: Invalid image header by channel number query";
    }
    return m_imagePage->getNumChannels();
}

slideio::DataType slideio::GDALScene::getChannelDataType(int channel) const
{
    if (m_imagePage == nullptr) {
        RAISE_RUNTIME_ERROR << "GDALDriver: Invalid image header by data type query";
    }
    return m_imagePage->getDataType();
}

slideio::Resolution  slideio::GDALScene::getResolution() const
{
    if (m_imagePage == nullptr) {
        RAISE_RUNTIME_ERROR << "GDALDriver: Invalid image header by data type query";
    }
    return m_imagePage->getResolution();
}

double slideio::GDALScene::getMagnification() const
{
    return 0;
}


std::string slideio::GDALScene::getName() const
{
    return std::string();
}

cv::Rect slideio::GDALScene::getRect() const
{
    if (m_imagePage == nullptr) {
        RAISE_RUNTIME_ERROR << "GDALDriver: Invalid image header by scene size query";
    }
    return { {}, m_imagePage->getSize() };
}

void slideio::GDALScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
	if (zSliceIndex != 0 || tFrameIndex != 0) {
		RAISE_RUNTIME_ERROR << "GDALDriver: Z-slice and T-frame indices are not supported";
	}
    if(m_imagePage == nullptr) {
        RAISE_RUNTIME_ERROR << "GDALDriver: Invalid file header by raster reading operation";
    }
    const int numChannels = m_imagePage->getNumChannels();
    const auto channelIndices = Tools::completeChannelList(componentIndices, numChannels);
    readClampedBlock(cv::Rect(cv::Point(), m_imagePage->getSize()), blockRect, blockSize, componentIndices,
        [&](const cv::Rect& rect, const cv::Size& size, cv::OutputArray block) {
            if (m_tiledDirectory != nullptr) {
                TileComposer::composeRect(this, channelIndices, rect, size, block);
            } else {
                readWholePageBlock(rect, size, channelIndices, block);
            }
        },
        output);
}

void slideio::GDALScene::readWholePageBlock(const cv::Rect& rect, const cv::Size& size,
                                            const std::vector<int>& channelIndices, cv::OutputArray block)
{
    const int numChannels = m_imagePage->getNumChannels();
    cv::Mat sceneRaster;
    if (Tools::isConsecutiveFromZero(channelIndices, numChannels)) {
        m_imagePage->readRaster(sceneRaster);
    } else {
        cv::Mat raster;
        m_imagePage->readRaster(raster);
        std::vector<cv::Mat> channels(channelIndices.size());
        int channelNum = 0;
        for (const auto& channelIndex : channelIndices) {
            cv::extractChannel(raster, channels[channelNum++], channelIndex);
        }
        cv::merge(channels, sceneRaster);
    }
    cv::Mat blockRaster = sceneRaster(rect);
    if ((size.width != rect.width) || (size.height != rect.height)) {
        cv::resize(blockRaster, blockRaster, size, 0, 0, cv::INTER_LINEAR);
    }
    blockRaster.copyTo(block);
}

int slideio::GDALScene::getTileCount(void* /*userData*/)
{
    const TiffDirectory& dir = *m_tiledDirectory;
    const int tilesX = (dir.width - 1) / dir.tileWidth + 1;
    const int tilesY = (dir.height - 1) / dir.tileHeight + 1;
    return tilesX * tilesY;
}

bool slideio::GDALScene::getTileRect(int tileIndex, cv::Rect& tileRect, void* /*userData*/)
{
    // Tiles are numbered row by row, as libtiff numbers a single-plane directory.
    // Edge tiles are clipped to the page, and readTile crops them to match.
    // Clipping only the rect is not enough: TileComposer resizes the whole tile
    // before it intersects it with the block, so for a scaled read the padding an
    // encoder writes past the page edge would be interpolated into the last real
    // pixels.
    tileRect = tileRectOf(tileIndex);
    return true;
}

bool slideio::GDALScene::readTile(int tileIndex, const std::vector<int>& channelIndices,
                                  cv::OutputArray tileRaster, void* /*userData*/)
{
    // The same call readTiledDir makes for every tile of the page, so a tile reads
    // exactly as it did when the page was decoded whole. A tile that fails to decode
    // still fails the read: the whole-page path never painted over a bad tile either.
    // For an interleaved directory every channel in file order is passed as the empty
    // list, which copies the decoded tile as it is; an explicit list costs a split and
    // a merge per tile. A planar directory reads one plane per listed channel, so the
    // empty list would read none: it always gets the explicit list.
    const TiffDirectory& dir = *m_tiledDirectory;
    static const std::vector<int> allChannels;
    const bool wholeTile = dir.interleaved && Tools::isConsecutiveFromZero(channelIndices, dir.channels);
    cv::Mat decoded;
    TiffTools::readTile(m_tiff->getHandle(), dir, tileIndex,
                        wholeTile ? allChannels : channelIndices, decoded);

    // TileComposer resizes whatever comes back to the tile rect, so a tile of the
    // wrong size or type would be stretched or silently dropped rather than fail.
    // A decoder may return the padded tile or, for JPEG 2000, just the valid part;
    // either is cropped to the valid part. Anything smaller is an error.
    const cv::Rect validRect = tileRectOf(tileIndex);
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

cv::Rect slideio::GDALScene::tileRectOf(int tileIndex) const
{
    const TiffDirectory& dir = *m_tiledDirectory;
    const int tilesX = (dir.width - 1) / dir.tileWidth + 1;
    const int x = (tileIndex % tilesX) * dir.tileWidth;
    const int y = (tileIndex / tilesX) * dir.tileHeight;
    return {x, y, std::min(dir.tileWidth, dir.width - x), std::min(dir.tileHeight, dir.height - y)};
}

void slideio::GDALScene::initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices,
                                         cv::OutputArray output)
{
    const int numChannels = static_cast<int>(channelIndices.size());
    output.create(blockSize, CV_MAKETYPE(CVTools::toOpencvType(m_tiledDirectory->dataType), numChannels));
    output.setTo(0);
}

slideio::Compression slideio::GDALScene::getCompression() const {
    if (m_imagePage == nullptr) {
        RAISE_RUNTIME_ERROR << "GDALDriver: Invalid image header by compression query";
    }
    return m_imagePage->getCompression();
}

slideio::MetadataFormat slideio::GDALScene::getMetadataFormat() const {
    return MetadataFormat::JSON;
}

std::string slideio::GDALScene::getRawMetadata() const {
    return m_imagePage->getMetadata();
}
