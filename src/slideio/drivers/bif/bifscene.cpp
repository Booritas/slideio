// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/bif/bifscene.hpp"
#include "slideio/drivers/bif/bifdescription.hpp"
#include "slideio/drivers/bif/bifreadcontext.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/levelinfo.hpp"
#include "slideio/core/tools/tools.hpp"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace slideio;

namespace
{
    // Output pixels [begin, end) of one axis whose storage pixels all lie in storage
    // tile `tile` along that axis.
    struct TileRun
    {
        int tile;
        int begin;
        int end;
    };

    std::vector<TileRun> tileRuns(const std::vector<int>& map, int tileExtent)
    {
        std::vector<TileRun> runs;
        for (int i = 0; i < static_cast<int>(map.size()); ++i) {
            const int tile = map[i] / tileExtent;
            if (runs.empty() || runs.back().tile != tile) {
                runs.push_back({ tile, i, i + 1 });
            }
            else {
                runs.back().end = i + 1;
            }
        }
        return runs;
    }

    // Output columns [begin, end) that read consecutive storage columns starting at
    // `source`, relative to their tile: one memcpy each.
    struct CopySpan
    {
        int begin;
        int end;
        int source;
    };

    std::vector<CopySpan> copySpans(const std::vector<int>& map, const TileRun& run, int tileOrigin)
    {
        std::vector<CopySpan> spans;
        for (int i = run.begin; i < run.end; ++i) {
            if (spans.empty() || map[i] != map[i - 1] + 1) {
                spans.push_back({ i, i + 1, map[i] - tileOrigin });
            }
            else {
                spans.back().end = i + 1;
            }
        }
        return spans;
    }
}

BIFScene::BIFScene(const std::string& path, const std::string& name, int sceneIndex,
                   std::vector<TiffDirectory> levels, bif::AoiGeometry geometry, bif::Aoi aoi,
                   bif::AcquisitionInfo acq)
    : m_filePath(path),
      m_driverId("BIF"),
      m_name(name),
      m_sceneIndex(sceneIndex),
      m_levelDirectories(std::move(levels)),
      m_geometry(std::move(geometry)),
      m_aoi(std::move(aoi)),
      m_acquisition(std::move(acq)),
      m_contextPool([path]() { return std::make_unique<bif::BIFReadContext>(path); })
{
    if (m_levelDirectories.empty()) {
        RAISE_RUNTIME_ERROR << "BIF: scene " << m_name << " has no pyramid levels";
    }
    const TiffDirectory& base = m_levelDirectories.front();
    // A plain Ventana TIFF's synthetic AOI covers whole tiles, so it can overhang the
    // raster; a stitched AOI was checked against the raster by BIFSlide already.
    const cv::Size size = m_geometry.size();
    m_rect = cv::Rect(0, 0, std::min(size.width, base.width), std::min(size.height, base.height));

    std::vector<double> magnifications;
    for (const auto& dir : m_levelDirectories) {
        magnifications.push_back(bif::parseLevelDescription(dir.description).value().magnification);
    }
    m_magnification = m_acquisition.magnification > 0 ? m_acquisition.magnification
                                                       : magnifications.front();
    const double scanRes = m_acquisition.scanResMicrons * 1.e-6;
    m_resolution = Resolution(scanRes, scanRes);
    m_compression = base.slideioCompression;
    m_colorProfile = ColorProfile(base.iccProfile);
    m_rawMetadata = m_aoi.imageInfoXml;
    m_metadataFormat = m_rawMetadata.empty() ? MetadataFormat::None : MetadataFormat::XML;

    for (int level = 0; level < static_cast<int>(m_levelDirectories.size()); ++level) {
        const int d = static_cast<int>(std::lround(magnifications.front() / magnifications[level]));
        m_downsamples.push_back(d);
        const TiffDirectory& dir = m_levelDirectories[level];
        m_levels.emplace_back(level, Size(m_rect.width / d, m_rect.height / d), 1. / d,
                              magnifications[level], Size(dir.tileWidth, dir.tileHeight));
    }
}

void BIFScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
                                            const std::vector<int>& channelIndices, int zSliceIndex,
                                            int tFrameIndex, cv::OutputArray output)
{
    int level = 0;
    if (blockRect.width > 0 && blockRect.height > 0) {
        const double zoomX = double(blockSize.width) / double(blockRect.width);
        const double zoomY = double(blockSize.height) / double(blockRect.height);
        level = Tools::findZoomLevel(std::max(zoomX, zoomY), static_cast<int>(m_downsamples.size()),
                                     [this](int index) { return 1.0 / double(m_downsamples[index]); });
    }
    const double scale = 1.0 / double(m_downsamples[level]);
    cv::Rect levelRect;
    Tools::scaleRect(blockRect, scale, scale, levelRect);
    readResampledLevelBlockChannelsEx(level, levelRect, blockSize, channelIndices, zSliceIndex, tFrameIndex,
                                      output);
}

void BIFScene::readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect, const cv::Size& blockSize,
                                                 const std::vector<int>& channelIndices, int zSliceIndex,
                                                 int tFrameIndex, cv::OutputArray output)
{
    if (zSliceIndex != 0 || tFrameIndex != 0) {
        RAISE_RUNTIME_ERROR << "BIF: the scene has one Z slice and one time frame";
    }
    validateLevel(level);
    for (int channel : channelIndices) {
        if (channel < 0 || channel >= getNumChannels()) {
            RAISE_RUNTIME_ERROR << "BIF: invalid channel index " << channel;
        }
    }
    const cv::Rect bounds(cv::Point(0, 0), m_levels[level].getSize());
    readClampedBlock(bounds, levelRect, blockSize, channelIndices,
        [&](const cv::Rect& validRect, const cv::Size& partSize, cv::OutputArray part) {
            // One borrow for the whole read: every tile below is decoded on this handle.
            auto borrow = m_contextPool.acquire();
            cv::Mat raster;
            renderLevelRect(level, validRect, channelIndices, raster, borrow);
            if (partSize == validRect.size()) {
                raster.copyTo(part);
            }
            else {
                cv::resize(raster, part, partSize, 0, 0, cv::INTER_AREA);
            }
        },
        output);
}

void BIFScene::renderLevelRect(int level, const cv::Rect& rect, const std::vector<int>& channels,
                               cv::Mat& out, ContextPool::Borrow& ctx) const
{
    const TiffDirectory& dir = m_levelDirectories[level];
    const int d = m_downsamples[level];
    const std::vector<int> columns = m_geometry.mapColumns(rect.x, rect.width, d, dir.width);
    const std::vector<int> rows = m_geometry.mapRows(rect.y, rect.height, d, dir.height);
    const int tilesAcross = (dir.width + dir.tileWidth - 1) / dir.tileWidth;
    const int planes = channels.empty() ? getNumChannels() : static_cast<int>(channels.size());
    out.create(rect.size(), CV_MAKETYPE(CV_8U, planes));
    const size_t pixelSize = out.elemSize();

    libtiff::TIFF* tiff = ctx.as<bif::BIFReadContext>().keeper.getHandle();
    const std::vector<TileRun> rowRuns = tileRuns(rows, dir.tileHeight);
    // The copy plan of each column run is the same for every row run: build it once.
    std::vector<std::pair<TileRun, std::vector<CopySpan>>> columnRuns;
    for (const TileRun& run : tileRuns(columns, dir.tileWidth)) {
        columnRuns.emplace_back(run, copySpans(columns, run, run.tile * dir.tileWidth));
    }
    cv::Mat tile;
    for (const TileRun& rowRun : rowRuns) {
        const int tileY = rowRun.tile * dir.tileHeight;
        for (const auto& [columnRun, spans] : columnRuns) {
            TiffTools::readTile(tiff, dir, rowRun.tile * tilesAcross + columnRun.tile, channels, tile);
            // readTile returns the whole tile, padding included, but an edge tile cropped
            // to the raster would do as well: maps are clamped to the raster.
            const int lastColumn = columns[columnRun.end - 1] - columnRun.tile * dir.tileWidth;
            const int lastRow = rows[rowRun.end - 1] - tileY;
            if (tile.type() != out.type() || lastColumn >= tile.cols || lastRow >= tile.rows) {
                RAISE_RUNTIME_ERROR << "BIF: tile " << rowRun.tile * tilesAcross + columnRun.tile
                    << " of level " << level << " decoded as " << tile.cols << "x" << tile.rows
                    << " with " << tile.channels() << " channels";
            }
            for (int y = rowRun.begin; y < rowRun.end; ++y) {
                const uint8_t* source = tile.ptr<uint8_t>(rows[y] - tileY);
                uint8_t* target = out.ptr<uint8_t>(y);
                for (const CopySpan& span : spans) {
                    std::memcpy(target + span.begin * pixelSize, source + span.source * pixelSize,
                                (span.end - span.begin) * pixelSize);
                }
            }
        }
    }
}
