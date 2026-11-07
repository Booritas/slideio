// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/mrxs/mrxsgeometry.hpp"
#include "slideio/drivers/mrxs/mrxserrors.hpp"
#include "slideio/core/tools/filereader.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_set>
#include <zlib.h>

using namespace slideio::mrxs;

namespace
{
    const int64_t kRecordBytes = 9;

    int32_t readLE32(const uint8_t* p)
    {
        const uint32_t v = uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
        int32_t result;
        std::memcpy(&result, &v, sizeof(result));
        return result;
    }

    // Inflates a zlib-wrapped stream into exactly `expected` bytes. Output beyond that is
    // corrupt, and is detected without ever growing the buffer past expected + 1.
    std::vector<uint8_t> inflateExactly(const std::vector<uint8_t>& packed, size_t expected)
    {
        std::vector<uint8_t> out(expected + 1);
        z_stream stream{};
        if (inflateInit(&stream) != Z_OK) {
            MRXS_CORRUPT << "cannot initialise zlib for the position map";
        }
        stream.next_in = const_cast<Bytef*>(packed.data());
        stream.avail_in = static_cast<uInt>(packed.size());
        stream.next_out = out.data();
        stream.avail_out = static_cast<uInt>(out.size());
        const int status = inflate(&stream, Z_FINISH);
        const size_t produced = out.size() - stream.avail_out;
        inflateEnd(&stream);
        if (status != Z_STREAM_END) {
            MRXS_CORRUPT << "the compressed position map is not a complete zlib stream ("
                << (stream.msg ? stream.msg : "zlib status " + std::to_string(status)) << ")";
        }
        if (produced != expected) {
            MRXS_CORRUPT << "the compressed position map inflates to " << (produced > expected ? "more than " : "")
                << produced << " bytes; the camera grid needs " << expected;
        }
        out.resize(expected);
        return out;
    }

    // Nominal original-lattice size, accumulated with truncation after every overlap
    // subtraction exactly as the format description 9.2 specifies.
    int64_t nominalSize(int count, int d, int imageSize, double overlap)
    {
        double total = 0;
        for (int i = 0; i < count; ++i) {
            if (i % d == d - 1 && i != count - 1) {
                total = std::trunc(total + imageSize - overlap);
            }
            else {
                total = total + imageSize;
            }
        }
        return static_cast<int64_t>(total);
    }
}

std::vector<CameraPosition> slideio::mrxs::decodePositionMap(const std::vector<uint8_t>& payload,
    PositionSource source, int px, int py, int64_t c0)
{
    const int64_t count = int64_t(px) * py;
    const int64_t expected = count * kRecordBytes;
    std::vector<uint8_t> unpacked;
    const std::vector<uint8_t>* raw = &payload;
    if (source == PositionSource::Zlib) {
        unpacked = inflateExactly(payload, static_cast<size_t>(expected));
        raw = &unpacked;
    }
    else if (static_cast<int64_t>(payload.size()) != expected) {
        MRXS_CORRUPT << "the position map has " << payload.size() << " bytes; the " << px << " x " << py
            << " camera grid needs " << expected;
    }
    std::vector<CameraPosition> positions(static_cast<size_t>(count));
    for (int64_t i = 0; i < count; ++i) {
        const uint8_t* record = raw->data() + i * kRecordBytes;
        if (record[0] > 1) {
            MRXS_CORRUPT << "position record " << i << " has flag " << int(record[0]) << " (only 0 and 1 are valid)";
        }
        // The flag is validated but not interpreted: it is not an activity mask.
        positions[i].x = int64_t(readLE32(record + 1)) * c0;
        positions[i].y = int64_t(readLE32(record + 5)) * c0;
    }
    return positions;
}

std::vector<uint8_t> slideio::mrxs::readPayload(const SlideStructure& slide, const PayloadRef& ref)
{
    const FileReader reader(slide.metadata.dataFiles.at(ref.fileNumber).u8string());
    std::vector<uint8_t> data(static_cast<size_t>(ref.length));
    reader.readAt(static_cast<uint64_t>(ref.offset), data.data(), data.size());
    return data;
}

bool LevelGeometry::contributes(const Drawable& d, double x0, double y0, double x1, double y1) const
{
    const double right = d.x + std::ceil(tileWidth) + 1;
    const double bottom = d.y + std::ceil(tileHeight) + 1;
    return d.x - 1 < x1 && right > x0 && d.y - 1 < y1 && bottom > y0;
}

void LevelGeometry::buildIndex()
{
    m_binStart.clear();
    m_binItems.clear();
    if (m_drawables.empty()) {
        m_binColumns = m_binRows = 0;
        m_binStart.assign(1, 0);
        return;
    }
    // Square bins a few tiles wide. A drawable is entered in every bin its contributing
    // extent touches, so duplication is bounded by the overlap between neighbours.
    const double extentW = std::ceil(tileWidth) + 2;
    const double extentH = std::ceil(tileHeight) + 2;
    m_binSize = std::max(4.0 * std::max(extentW, extentH), 1.0);
    double minX = std::numeric_limits<double>::max(), minY = minX;
    double maxX = std::numeric_limits<double>::lowest(), maxY = maxX;
    for (const Drawable& d : m_drawables) {
        minX = std::min(minX, d.x - 1);
        minY = std::min(minY, d.y - 1);
        maxX = std::max(maxX, d.x - 1 + extentW);
        maxY = std::max(maxY, d.y - 1 + extentH);
    }
    m_binOriginX = minX;
    m_binOriginY = minY;
    m_binColumns = static_cast<int>(std::floor((maxX - minX) / m_binSize)) + 1;
    m_binRows = static_cast<int>(std::floor((maxY - minY) / m_binSize)) + 1;
    const size_t bins = size_t(m_binColumns) * size_t(m_binRows);
    std::vector<uint32_t> counts(bins + 1, 0);
    const auto forEachBin = [&](const Drawable& d, auto&& visit) {
        const int c0 = static_cast<int>(std::floor((d.x - 1 - m_binOriginX) / m_binSize));
        const int r0 = static_cast<int>(std::floor((d.y - 1 - m_binOriginY) / m_binSize));
        const int c1 = static_cast<int>(std::floor((d.x - 1 + extentW - m_binOriginX) / m_binSize));
        const int r1 = static_cast<int>(std::floor((d.y - 1 + extentH - m_binOriginY) / m_binSize));
        for (int r = std::max(r0, 0); r <= std::min(r1, m_binRows - 1); ++r) {
            for (int c = std::max(c0, 0); c <= std::min(c1, m_binColumns - 1); ++c) {
                visit(size_t(r) * m_binColumns + c);
            }
        }
    };
    for (const Drawable& d : m_drawables) {
        forEachBin(d, [&](size_t bin) { ++counts[bin + 1]; });
    }
    for (size_t i = 1; i <= bins; ++i) {
        counts[i] += counts[i - 1];
    }
    if (counts[bins] > std::numeric_limits<uint32_t>::max()) {
        MRXS_LIMIT << "the spatial index of a level would exceed 2^32 entries";
    }
    m_binStart = counts;
    m_binItems.resize(counts[bins]);
    std::vector<uint32_t> fill(counts.begin(), counts.end() - 1);
    for (uint32_t i = 0; i < m_drawables.size(); ++i) {
        forEachBin(m_drawables[i], [&](size_t bin) { m_binItems[fill[bin]++] = i; });
    }
}

std::vector<uint32_t> LevelGeometry::query(double x0, double y0, double x1, double y1) const
{
    std::vector<uint32_t> hits;
    if (m_drawables.empty() || x1 <= x0 || y1 <= y0) {
        return hits;
    }
    const auto clampBin = [](double value, int count) {
        return static_cast<int>(std::clamp(std::floor(value), 0.0, double(count - 1)));
    };
    const int c0 = clampBin((x0 - m_binOriginX) / m_binSize, m_binColumns);
    const int c1 = clampBin((x1 - m_binOriginX) / m_binSize, m_binColumns);
    const int r0 = clampBin((y0 - m_binOriginY) / m_binSize, m_binRows);
    const int r1 = clampBin((y1 - m_binOriginY) / m_binSize, m_binRows);
    for (int r = r0; r <= r1; ++r) {
        for (int c = c0; c <= c1; ++c) {
            const size_t bin = size_t(r) * m_binColumns + c;
            for (uint32_t k = m_binStart[bin]; k < m_binStart[bin + 1]; ++k) {
                const uint32_t i = m_binItems[k];
                if (contributes(m_drawables[i], x0, y0, x1, y1)) {
                    hits.push_back(i);
                }
            }
        }
    }
    std::sort(hits.begin(), hits.end());
    hits.erase(std::unique(hits.begin(), hits.end()), hits.end());
    // Painting order: descending grid row, then descending grid column. Never file order.
    std::sort(hits.begin(), hits.end(), [this](uint32_t a, uint32_t b) {
        const Drawable& da = m_drawables[a];
        const Drawable& db = m_drawables[b];
        return da.gridRow != db.gridRow ? da.gridRow > db.gridRow : da.gridColumn > db.gridColumn;
    });
    return hits;
}

size_t LevelGeometry::memoryBytes() const
{
    return m_drawables.capacity() * sizeof(Drawable) + m_binStart.capacity() * sizeof(uint32_t)
        + m_binItems.capacity() * sizeof(uint32_t);
}

Geometry Geometry::build(const SlideStructure& slide)
{
    const SlideMetadata& meta = slide.metadata;
    const int px = meta.nx / meta.d;
    const int py = meta.ny / meta.d;
    if (slide.positionSource != PositionSource::None) {
        const std::vector<uint8_t> payload = readPayload(slide, slide.positions);
        return build(slide, decodePositionMap(payload, slide.positionSource, px, py, meta.levels[0].concat), true);
    }
    // No map: nominal positions, truncated toward zero and deliberately not scaled by C0
    // (format description 8.4).
    const LevelMetadata& l0 = meta.levels[0];
    std::vector<CameraPosition> positions(size_t(px) * py);
    for (int yp = 0; yp < py; ++yp) {
        for (int xp = 0; xp < px; ++xp) {
            CameraPosition& p = positions[size_t(yp) * px + xp];
            p.x = static_cast<int64_t>(std::trunc(xp * (double(l0.imageWidth) * meta.d - l0.overlapX)));
            p.y = static_cast<int64_t>(std::trunc(yp * (double(l0.imageHeight) * meta.d - l0.overlapY)));
        }
    }
    return build(slide, positions, false);
}

Geometry Geometry::build(const SlideStructure& slide, const std::vector<CameraPosition>& positions, bool positionMap)
{
    const SlideMetadata& meta = slide.metadata;
    const int d = meta.d;
    const LevelMetadata& l0 = meta.levels[0];
    Geometry g;
    g.cameraColumns = meta.nx / d;
    g.cameraRows = meta.ny / d;
    if (positions.size() != size_t(g.cameraColumns) * g.cameraRows) {
        MRXS_CORRUPT << "expected " << size_t(g.cameraColumns) * g.cameraRows << " camera positions, got "
            << positions.size();
    }
    // A slide-wide decision, not per level (format description 9.3).
    g.positionAware = positionMap || l0.overlapX != 0 || l0.overlapY != 0;
    g.latticeWidth = nominalSize(meta.nx, d, l0.imageWidth, l0.overlapX);
    g.latticeHeight = nominalSize(meta.ny, d, l0.imageHeight, l0.overlapY);

    // Cameras accepted at stored level 0; every later level consults them, so they are
    // built first even if only a coarse level is ever read (format description 10.3).
    std::vector<char> active(positions.size(), 0);
    for (size_t l = 0; l < meta.levels.size(); ++l) {
        const LevelMetadata& lm = meta.levels[l];
        LevelGeometry level;
        level.concat = lm.concat;
        level.downsample = lm.concat / l0.concat;
        level.width = g.latticeWidth / lm.concat;
        level.height = g.latticeHeight / lm.concat;
        if (level.width <= 0 || level.height <= 0 || level.width > std::numeric_limits<int32_t>::max()
            || level.height > std::numeric_limits<int32_t>::max()) {
            MRXS_UNSUPPORTED << "zoom level " << l << " is " << level.width << " x " << level.height
                << " pixels, outside what the API can represent";
        }
        const int64_t positionsPerImage = std::max<int64_t>(1, lm.concat / d);
        if (g.positionAware) {
            level.gridDivisor = std::min<int64_t>(lm.concat, d);
            level.tilesPerImage = static_cast<int>(positionsPerImage);
            level.positionsPerTile = 1;
        }
        else {
            level.gridDivisor = lm.concat;
            level.tilesPerImage = 1;
            level.positionsPerTile = static_cast<int>(positionsPerImage);
        }
        if (level.tilesPerImage > std::numeric_limits<uint16_t>::max()) {
            MRXS_UNSUPPORTED << "zoom level " << l << " splits an image into " << level.tilesPerImage << " subtiles per axis";
        }
        level.tileWidth = double(lm.imageWidth) / level.tilesPerImage;
        level.tileHeight = double(lm.imageHeight) / level.tilesPerImage;

        const std::vector<ImageRecord>& records = slide.levels[l];
        if (records.size() > std::numeric_limits<uint32_t>::max()) {
            MRXS_LIMIT << "zoom level " << l << " has more than 2^32 records";
        }
        std::unordered_set<int64_t> gridCells;
        const int q = level.positionsPerTile;
        for (uint32_t r = 0; r < records.size(); ++r) {
            const ImageRecord& rec = records[r];
            for (int yi = 0; yi < level.tilesPerImage; ++yi) {
                const int64_t yy = rec.y + int64_t(yi) * d;
                if (yy >= meta.ny) break;
                for (int xi = 0; xi < level.tilesPerImage; ++xi) {
                    const int64_t xx = rec.x + int64_t(xi) * d;
                    if (xx >= meta.nx) break;
                    const int64_t xp = xx / d;
                    const int64_t yp = yy / d;
                    const size_t cp = size_t(yp) * g.cameraColumns + size_t(xp);
                    if (l == 0) {
                        // A camera at (0, 0) coordinates other than the first is a missing
                        // photo, not one placed at the origin (format description 10.3).
                        if (positions[cp].x == 0 && positions[cp].y == 0 && (xp != 0 || yp != 0)) {
                            continue;
                        }
                        active[cp] = 1;
                    }
                    else {
                        bool any = false;
                        for (int64_t cy = yp; cy < std::min<int64_t>(yp + q, g.cameraRows) && !any; ++cy) {
                            for (int64_t cx = xp; cx < std::min<int64_t>(xp + q, g.cameraColumns); ++cx) {
                                if (active[size_t(cy) * g.cameraColumns + size_t(cx)]) { any = true; break; }
                            }
                        }
                        if (!any) continue;
                    }
                    const int64_t p0x = positions[cp].x + int64_t(l0.imageWidth) * (xx - xp * d);
                    const int64_t p0y = positions[cp].y + int64_t(l0.imageHeight) * (yy - yp * d);
                    Drawable dr;
                    dr.x = double(p0x) / double(lm.concat);
                    dr.y = double(p0y) / double(lm.concat);
                    dr.record = r;
                    dr.xi = static_cast<uint16_t>(xi);
                    dr.yi = static_cast<uint16_t>(yi);
                    dr.gridRow = static_cast<int32_t>(rec.y / level.gridDivisor + yi);
                    dr.gridColumn = static_cast<int32_t>(rec.x / level.gridDivisor + xi);
                    const int64_t cell = (int64_t(dr.gridRow) << 32) | uint32_t(dr.gridColumn);
                    if (!gridCells.insert(cell).second) {
                        MRXS_CORRUPT << "zoom level " << l << " places two subtiles at grid cell ("
                            << dr.gridColumn << ", " << dr.gridRow << ")";
                    }
                    level.m_drawables.push_back(dr);
                }
            }
        }
        level.m_drawables.shrink_to_fit();
        level.buildIndex();
        if (l == 0 && !level.m_drawables.empty()) {
            double x0 = std::numeric_limits<double>::max(), y0 = x0;
            double x1 = std::numeric_limits<double>::lowest(), y1 = x1;
            for (const Drawable& dr : level.m_drawables) {
                x0 = std::min(x0, dr.x);
                y0 = std::min(y0, dr.y);
                x1 = std::max(x1, dr.x + level.tileWidth);
                y1 = std::max(y1, dr.y + level.tileHeight);
            }
            g.bounds0 = cv::Rect2d(x0, y0, x1 - x0, y1 - y0);
        }
        g.levels.push_back(std::move(level));
    }
    return g;
}
