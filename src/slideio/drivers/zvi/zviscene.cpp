// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/base/exceptions.hpp"
#include "slideio/base/log.hpp"
#include "slideio/drivers/zvi/zviscene.hpp"
#include "slideio/drivers/zvi/zvislide.hpp"
#include "slideio/drivers/zvi/zvitags.hpp"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <variant>
#include <vector>

#include "zviutils.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include "slideio/imagetools/imagetools.hpp"

using namespace slideio;

namespace
{
    // An upper bound on the tile grid a file may declare. The largest mosaic
    // in the test set is a few hundred tiles; this only has to be small enough
    // that a corrupt grid cannot ask for an unreasonable allocation.
    constexpr int64_t MAX_TILE_COUNT = 1 << 20;

    // "/Image/Item(<n>)" -> n. Returns -1 for anything else, including the
    // storages nested inside an item ("/Image/Item(3)/Tags") and the sibling
    // "/Image/DisplayItem".
    int imageItemIndex(const std::string& path)
    {
        static const std::string prefix = "/Image/Item(";
        if (path.size() <= prefix.size() || path.compare(0, prefix.size(), prefix) != 0) {
            return -1;
        }
        if (path.back() != ')') {
            return -1;
        }
        const std::string digits = path.substr(prefix.size(), path.size() - prefix.size() - 1);
        // 9 digits keep the conversion inside int without a range check.
        if (digits.empty() || digits.size() > 9) {
            return -1;
        }
        for (const char c : digits) {
            if (c < '0' || c > '9') {
                return -1;
            }
        }
        return std::stoi(digits);
    }
}

ZVIScene::ZVIScene(const std::string& filePath, const std::string& driverId) :
    m_filePath(filePath),
#if defined(WIN32)
    m_Doc(Tools::toWstring(filePath)),
#else
    m_Doc(filePath),
#endif
    m_SceneName("Unknown"),
	m_driverId(driverId)
{
    init();
}

std::string ZVIScene::getFilePath() const
{
    return m_filePath;
}

cv::Rect ZVIScene::getRect() const
{
    return cv::Rect(0, 0, m_Width, m_Height);
}

int ZVIScene::getNumChannels() const
{
    return m_ChannelCount;
}

int ZVIScene::getNumZSlices() const
{
    return m_ZSliceCount;
}

int ZVIScene::getNumTFrames() const
{
    return m_TFrameCount;
}

double ZVIScene::getZSliceResolution() const
{
    return m_ZSliceRes;
}

double ZVIScene::getTFrameResolution() const
{
    return 0;
}

void ZVIScene::validateChannelIndex(int channel) const
{
    if (channel < 0 || channel >= m_ChannelCount) {
        RAISE_RUNTIME_ERROR << "Invalid channel index:" << channel << ". Number of channels:" << m_ChannelCount;
    }
}

slideio::DataType ZVIScene::getChannelDataType(int channel) const
{
    validateChannelIndex(channel);
    return m_ChannelDataTypes[channel];
}

std::string ZVIScene::getChannelName(int channel) const
{
    validateChannelIndex(channel);
    return m_ChannelNames[channel];
}

Resolution ZVIScene::getResolution() const
{
    return m_res;
}

double ZVIScene::getMagnification() const
{
    return 0;
}

void ZVIScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
                                            const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex,
                                            cv::OutputArray output)
{
    TilerData userData;
    userData.zSliceIndex = zSliceIndex;
    const std::vector<int> channelIndices = Tools::completeChannelList(componentIndices, getNumChannels());
    TileComposer::composeRect(this, channelIndices, blockRect, blockSize, output, &userData);
}


std::string ZVIScene::getName() const
{
    return m_SceneName;
}

Compression ZVIScene::getCompression() const
{
    return m_Compression;
}

// The number of tiles that exist, not the product of the two counts read from
// the file: computeTiles() is what reconciles the declared grid with what can
// be allocated, and m_Tiles is what every tile index addresses.
int ZVIScene::getTileCount(void* userData)
{
    return static_cast<int>(m_Tiles.size());
}

bool ZVIScene::getTileRect(int tileIndex, cv::Rect& tileRect, void* userData)
{
    if (tileIndex < 0 || tileIndex >= static_cast<int>(m_Tiles.size())) {
        return false;
    }
    tileRect = m_Tiles[tileIndex].getRect();
    return true;
}

bool ZVIScene::readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
                        void* userData)
{
    if (tileIndex < 0 || tileIndex >= static_cast<int>(m_Tiles.size())) {
        return false;
    }
    TilerData* data = (TilerData*)userData;
    int slice = data->zSliceIndex;
    ZVITile& tile = m_Tiles[tileIndex];
    return tile.readTile(channelIndices, tileRaster, slice, m_Doc);
}


ZVIPixelFormat ZVIScene::getPixelFormat() const
{
    return (m_PixelFormat == ZVIPixelFormat::PF_UNKNOWN) ? m_ImageItems[0].getPixelFormat() : m_PixelFormat;
}

void ZVIScene::alignChannelInfoToPixelFormat()
{
    if (m_ChannelCount == 1 && !m_ImageItems.empty())
    {
        ZVIPixelFormat pixelFormat = getPixelFormat();
        switch (pixelFormat)
        {
        case ZVIPixelFormat::PF_BGR:
            m_ChannelCount = 3;
            m_ChannelNames.resize(m_ChannelCount);
            m_ChannelDataTypes.resize(m_ChannelCount);
            std::fill(m_ChannelDataTypes.begin(), m_ChannelDataTypes.end(), DataType::DT_Byte);
            m_ChannelNames[0] = "blue";
            m_ChannelNames[1] = "green";
            m_ChannelNames[2] = "red";
            break;
        case ZVIPixelFormat::PF_BGR16:
            m_ChannelCount = 3;
            m_ChannelDataTypes.resize(m_ChannelCount);
            std::fill(m_ChannelDataTypes.begin(), m_ChannelDataTypes.end(), DataType::DT_Int16);
            m_ChannelNames.resize(m_ChannelCount);
            m_ChannelNames[0] = "blue";
            m_ChannelNames[1] = "green";
            m_ChannelNames[2] = "red";
            break;
        case ZVIPixelFormat::PF_BGR32:
            m_ChannelCount = 3;
            m_ChannelDataTypes.resize(m_ChannelCount);
            std::fill(m_ChannelDataTypes.begin(), m_ChannelDataTypes.end(), DataType::DT_Int32);
            m_ChannelNames.resize(m_ChannelCount);
            m_ChannelNames[0] = "blue";
            m_ChannelNames[1] = "green";
            m_ChannelNames[2] = "red";
            break;
        case ZVIPixelFormat::PF_BGRA:
            m_ChannelCount = 4;
            m_ChannelDataTypes.resize(m_ChannelCount);
            std::fill(m_ChannelDataTypes.begin(), m_ChannelDataTypes.end(), DataType::DT_Byte);
            m_ChannelNames.resize(m_ChannelCount);
            m_ChannelNames[0] = "blue";
            m_ChannelNames[1] = "green";
            m_ChannelNames[2] = "red";
            m_ChannelNames[3] = "alpha";
            break;
        case ZVIPixelFormat::PF_UINT8:
        case ZVIPixelFormat::PF_INT16:
        case ZVIPixelFormat::PF_INT32:
        case ZVIPixelFormat::PF_FLOAT:
        case ZVIPixelFormat::PF_DOUBLE:
            break;
        case ZVIPixelFormat::PF_UNKNOWN:
        default:
            RAISE_RUNTIME_ERROR << "ZVIImageDriver: Invalid pixel format: " << (int)pixelFormat 
                << " for file " << m_filePath;
        }
    }
}

void ZVIScene::computeSceneDimensions()
{
    // A dimension is as large as the number of distinct indices the image
    // items actually use, not as the largest index plus one. An index nothing
    // is stored under -- a channel the document does not carry, an item that
    // could not be read -- would otherwise be advertised as part of the scene
    // while every read of it failed in ZVITile::getImageItem(). Renumbering
    // the surviving indices densely is what the driver already did for a
    // missing leading channel; the same has to hold for a gap anywhere.
    std::vector<int> channels;
    std::vector<int> zSlices;
    std::vector<int> tFrames;
    channels.reserve(m_ImageItems.size());
    zSlices.reserve(m_ImageItems.size());
    tFrames.reserve(m_ImageItems.size());
    for (const auto& imageItem : m_ImageItems)
    {
        channels.push_back(imageItem.getCIndex());
        zSlices.push_back(imageItem.getZIndex());
        tFrames.push_back(imageItem.getTIndex());
    }
    const auto distinct = [](std::vector<int>& values) {
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());
    };
    distinct(channels);
    distinct(zSlices);
    distinct(tFrames);

    const auto rank = [](const std::vector<int>& values, int value) {
        return static_cast<int>(
            std::lower_bound(values.begin(), values.end(), value) - values.begin());
    };
    for (auto&& imageItem : m_ImageItems)
    {
        imageItem.setCIndex(rank(channels, imageItem.getCIndex()));
        imageItem.setZIndex(rank(zSlices, imageItem.getZIndex()));
        imageItem.setTIndex(rank(tFrames, imageItem.getTIndex()));
    }

    m_ChannelCount = static_cast<int>(channels.size());
    m_ZSliceCount = static_cast<int>(zSlices.size());
    m_TFrameCount = static_cast<int>(tFrames.size());
    m_ChannelNames.resize(m_ChannelCount);
    m_ChannelDataTypes.resize(m_ChannelCount);

    for (auto&& imageItem : m_ImageItems)
    {
        const int channelIndex = imageItem.getCIndex();
        const std::string channelName = imageItem.getChannelName();
        if (!channelName.empty())
            m_ChannelNames[channelIndex] = channelName;
        m_ChannelDataTypes[channelIndex] = imageItem.getDataType();
    }

    // Expose per-channel display hints from the first image item that
    // carried them, translated into the generic channel-attribute keys other
    // slideio drivers (CZI, ETS/VSI, OME-TIFF, SCN, PKE) already publish.
    // This lets downstream consumers — viewer, converters — apply per-format
    // colors uniformly through Scene::getChannelAttributes() without knowing
    // ZVI exists. Multichannel Colour is a Windows-style packed BGR int
    // (0x00BBGGRR); unpack to canonical "#RRGGBB" hex. Channel 0 and white
    // (0xFFFFFF) are treated as "unset" — both mean "no preference" in Zen.
    std::vector<bool> channelAttrsSet(static_cast<size_t>(m_ChannelCount), false);
    for (const auto& imageItem : m_ImageItems) {
        const int channelIndex = imageItem.getCIndex();
        if (channelIndex < 0 || channelIndex >= m_ChannelCount) {
            continue;
        }
        if (channelAttrsSet[static_cast<size_t>(channelIndex)]) {
            continue;
        }
        const int color = imageItem.getMultichannelColour();
        if (color != 0) {
            const unsigned int r = static_cast<unsigned int>(color        & 0xFF);
            const unsigned int g = static_cast<unsigned int>((color >>  8) & 0xFF);
            const unsigned int b = static_cast<unsigned int>((color >> 16) & 0xFF);
            char buf[8];
            std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
            setChannelAttribute(channelIndex, "Color", std::string(buf));
        }
        const double em = imageItem.getEmissionWavelength();
        if (em > 0.0) {
            setChannelAttribute(channelIndex, "EmissionWavelength", em);
        }
        const double ex = imageItem.getExcitationWavelength();
        if (ex > 0.0) {
            setChannelAttribute(channelIndex, "ExcitationWavelength", ex);
        }
        const std::string reflector = imageItem.getReflector();
        if (!reflector.empty()) {
            setChannelAttribute(channelIndex, "Reflector", reflector);
        }
        channelAttrsSet[static_cast<size_t>(channelIndex)] = true;
    }

    alignChannelInfoToPixelFormat();
}

// The indices of the image items the document actually holds, in ascending
// order.
//
// {RawCount} in /Image/Contents is not a reliable item count: files exist
// whose item storages are fewer than it declares, or are numbered with gaps.
// Deriving the item list from the declared count instead of from the document
// made a single absent storage ("Invalid stream path: /Image/Item(30)/
// Contents") cost the caller the whole file. Bio-Formats' ZeissZVIReader
// enumerates the document the same way and ignores {RawCount} entirely.
std::vector<int> ZVIScene::findImageItemIndices()
{
    std::vector<int> indices;
    for (auto it = m_Doc.begin(); it != m_Doc.end(); ++it)
    {
        const std::string storagePath = it->string();
        const int index = imageItemIndex(storagePath);
        if (index < 0) {
            continue;
        }
        // A storage without a <Contents> stream carries no raster and no
        // geometry: there is nothing for readContents() to read.
        if (!it->path_exist(storagePath + "/Contents")) {
            SLIDEIO_LOG(WARNING) << "ZVIImageDriver: " << storagePath
                << " has no Contents stream. The item is skipped.";
            continue;
        }
        indices.push_back(index);
    }
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    return indices;
}

void ZVIScene::readImageItems()
{
    const std::vector<int> itemIndices = findImageItemIndices();
    if (itemIndices.empty()) {
        RAISE_RUNTIME_ERROR << "ZVIImageDriver: no image item with a Contents stream found in "
            << m_filePath << ". /Image/Contents declares " << m_RawCount << " items.";
    }
    if (static_cast<int>(itemIndices.size()) != m_RawCount) {
        SLIDEIO_LOG(WARNING) << "ZVIImageDriver: /Image/Contents declares " << m_RawCount
            << " image items, the document contains " << itemIndices.size()
            << " (indices " << itemIndices.front() << ".." << itemIndices.back()
            << "). The items present in the document are used.";
    }

    m_ImageItems.clear();
    m_ImageItems.reserve(itemIndices.size());
    for (const int itemIndex : itemIndices)
    {
        ZVIImageItem item;
        item.setItemIndex(itemIndex);
        try {
            item.readItemInfo(m_Doc);
        }
        catch (const std::exception& e) {
            // One unreadable item must not cost the caller the other scenes:
            // the raster of the items that did parse is still readable.
            SLIDEIO_LOG(WARNING) << "ZVIImageDriver: /Image/Item(" << itemIndex
                << ") of " << m_filePath << " cannot be read and is skipped: " << e.what();
            continue;
        }
        const int validBits = item.getValidBits();
        if (validBits==0 || validBits==1) {
            m_Compression = Compression::Jpeg;
        }
        m_ImageItems.push_back(std::move(item));
    }

    if (m_ImageItems.empty()) {
        RAISE_RUNTIME_ERROR << "ZVIImageDriver: none of the " << itemIndices.size()
            << " image items of " << m_filePath << " could be read.";
    }
}

void ZVIScene::parseImageInfo()
{
    ZVIUtils::StreamKeeper stream(m_Doc, "/Image/Contents");
    ZVIUtils::skipItems(stream, 4);
    m_Width = ZVIUtils::readIntItem(stream);
    m_Height = ZVIUtils::readIntItem(stream);
    ZVIUtils::skipItem(stream);
    m_PixelFormat = (ZVIPixelFormat)ZVIUtils::readIntItem(stream);
    m_RawCount = ZVIUtils::readIntItem(stream);
}

void ZVIScene::computeTiles()
{
    // {ImageCountU}/{ImageCountV} are two unvalidated 32-bit values read from
    // the file. Their product is what sizes m_Tiles, so it has to be computed
    // where it cannot overflow: 0x10000 x 0x10000 wraps a signed int to zero,
    // which left an empty m_Tiles that the loop below then indexed far past
    // its end. One tile covering the whole image is what a file without those
    // tags means, and the only fallback that can be read.
    const int64_t declaredTiles =
        static_cast<int64_t>(m_TileCountX) * static_cast<int64_t>(m_TileCountY);
    if (m_TileCountX < 1 || m_TileCountY < 1 || declaredTiles > MAX_TILE_COUNT)
    {
        SLIDEIO_LOG(WARNING) << "ZVIImageDriver: " << m_filePath << " declares a "
            << m_TileCountX << "x" << m_TileCountY
            << " tile grid. A single tile is assumed.";
        m_TileCountX = 1;
        m_TileCountY = 1;
    }
    const int tileCount = m_TileCountX * m_TileCountY;
    m_Tiles.resize(tileCount);

    std::vector<int> w(m_TileCountX, -1);
    std::vector<int> h(m_TileCountY, -1);

    for (auto itemIndex = 0; itemIndex < m_ImageItems.size(); ++itemIndex)
    {
        ZVIImageItem& item = m_ImageItems[itemIndex];
        // An item whose tag stream could not be read has no tile position. On
        // a single tile image there is only one place it can belong. On a
        // mosaic there is not: placing it at (0,0) would let it shadow the
        // item that really belongs there -- ZVITile::getImageItem() returns
        // the first match for a (slice, channel) pair -- and serve its pixels
        // for the wrong part of the image.
        //
        // The resolved position is written back to the item: ZVITile::addItem()
        // reads the position from the item it is given, so resolving it only
        // here left the tile rejecting the item as a coordinate mismatch.
        if ((item.getTileIndexX() < 0 || item.getTileIndexY() < 0) && tileCount == 1)
        {
            item.setTileIndexX(0);
            item.setTileIndexY(0);
        }
        const int xIndex = item.getTileIndexX();
        const int yIndex = item.getTileIndexY();
        // The tile position comes from the item tag stream, the grid size from
        // /Image/Tags/Contents. Nothing in the format ties the two together, so
        // an out of range position is a corrupt-file case, not an invariant:
        // indexing m_Tiles with it wrote past the end of the vector.
        const int64_t tileIndex64 =
            static_cast<int64_t>(yIndex) * static_cast<int64_t>(m_TileCountX) + xIndex;
        if (xIndex < 0 || xIndex >= m_TileCountX || yIndex < 0 || yIndex >= m_TileCountY
            || tileIndex64 < 0 || tileIndex64 >= static_cast<int64_t>(m_Tiles.size()))
        {
            SLIDEIO_LOG(WARNING) << "ZVIImageDriver: /Image/Item(" << item.getItemIndex()
                << ") of " << m_filePath << " reports tile position (" << xIndex << ","
                << yIndex << "), outside the " << m_TileCountX << "x" << m_TileCountY
                << " tile grid. The item is skipped.";
            continue;
        }
        const int tileIndex = static_cast<int>(tileIndex64);
        ZVITile& tile = m_Tiles[tileIndex];
        tile.addItem(&item);
        if (w[xIndex] < 0)
            w[xIndex] = item.getWidth();
        if (h[yIndex] < 0)
            h[yIndex] = item.getHeight();
    }

    // A column or row that no item landed in keeps its -1 sentinel. It still
    // has to advance the running origin: contributing zero would stack every
    // tile after the gap on top of its neighbour and serve those pixels for
    // the wrong part of the image. Tiles of a ZVI mosaic are uniform, so the
    // size of any other column is the right stand-in.
    const auto fillMissingSizes = [](std::vector<int>& sizes, int total) {
        int known = 0;
        for (const int size : sizes) {
            if (size > 0) {
                known = size;
                break;
            }
        }
        if (known <= 0) {
            known = sizes.empty() ? 1 : std::max(1, total / static_cast<int>(sizes.size()));
        }
        for (int& size : sizes) {
            if (size <= 0) {
                size = known;
            }
        }
    };
    fillMissingSizes(w, m_Width);
    fillMissingSizes(h, m_Height);

    int yPos = 0;
    int tileIndex = 0;
    for (int yIndex = 0; yIndex < m_TileCountY; ++yIndex)
    {
        int xPos = 0;
        for (int xIndex = 0; xIndex < m_TileCountX; ++xIndex)
        {
            ZVITile& tile = m_Tiles[tileIndex];
            tile.setTilePosition(xPos, yPos);
            tile.finalize();
            xPos += w[xIndex];
            tileIndex++;
        }
        yPos += h[yIndex];
    }
}

void ZVIScene::init()
{
    Tools::throwIfPathNotExist(m_filePath, "ZVIScene::init");
    if (!m_Doc.good())
    {
        RAISE_RUNTIME_ERROR << "Cannot open compound file " << m_filePath;
    }
    parseImageInfo();
    readImageItems();
    computeSceneDimensions();
    parseImageTags();
    computeTiles();
    m_levels.resize(1);
    LevelInfo& level = m_levels[0];
    level.setLevel(0);
    level.setTileSize(Size(m_Width, m_Height));
    level.setSize(Size(m_Width, m_Height));
    // A tile that received no item keeps an empty rectangle, and the first
    // tile is not guaranteed to be one that did. Publishing 0x0 as the level
    // tile size would hand every caller a degenerate value; the full image
    // size already set above is the right fallback.
    for (const ZVITile& tile : m_Tiles) {
        const cv::Rect tileRect = tile.getRect();
        if (tileRect.width > 0 && tileRect.height > 0) {
            level.setTileSize(Tools::cvSizeToSize(tileRect.size()));
            break;
        }
    }
    level.setMagnification(getMagnification());
    level.setScale(1.);
}

static double scaleToResolution(double scale, int units)
{
    double res = scale;
    switch (units)
    {
    case 72: // Meter
        break;
    case 76: // Micrometer
    case 84: // Micrometer
        res /= 1.e6;
        break;
    case 77: // Namometer
        res /= 1.e9;
        break;
    }
    return res;
}

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"
#endif

void ZVIScene::parseImageTags()
{
    ZVIUtils::StreamKeeper stream(m_Doc, "/Image/Tags/Contents");
    const int version = ZVIUtils::readIntItem(stream);
    const int numTags = ZVIUtils::readIntItem(stream);
    double scaleX(0), scaleY(0), scaleZ(0);
    int unitsX(0), unitsY(0), unitsZ(0);

    for (int tagIndex = 0; tagIndex < numTags; ++tagIndex)
    {
        // {NumberOfTags} is not always the number of tags the stream holds, and
        // a tag the reader cannot decode must not cost the scene the tags read
        // before it -- geometry aside, these are metadata.
        if (ZVIUtils::bytesLeft(stream) < 2) {
            SLIDEIO_LOG(WARNING) << "ZVIImageDriver: /Image/Tags/Contents ends after "
                << tagIndex << " of " << numTags << " declared tags";
            break;
        }
        ZVIUtils::Variant tag;
        ZVITAG id = static_cast<ZVITAG>(0); // no tag has id 0
        try {
            tag = ZVIUtils::readItem(stream);
            id = static_cast<ZVITAG>(ZVIUtils::readIntItem(stream));
            ZVIUtils::skipItem(stream);
        }
        catch (const std::exception& e) {
            SLIDEIO_LOG(WARNING) << "ZVIImageDriver: stopped reading /Image/Tags/Contents after "
                << tagIndex << " of " << numTags << " tags: " << e.what();
            break;
        }
        if (tag.index() == 0)
            continue;

        try {
        switch (id)
        {
        case ZVITAG::ZVITAG_IMAGE_WIDTH:
            m_Width = std::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_IMAGE_HEIGHT:
            m_Height = std::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_IMAGE_COUNT_U:
            m_TileCountX = std::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_IMAGE_COUNT_V:
            m_TileCountY = std::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_SCALE_X:
            scaleX = std::get<double>(tag);
            break;
        case ZVITAG::ZVITAG_SCALE_UNIT_X:
            unitsX = std::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_SCALE_Y:
            scaleY = std::get<double>(tag);
            break;
        case ZVITAG::ZVITAG_SCALE_UNIT_Y:
            unitsY = std::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_SCALE_Z:
            scaleZ = std::get<double>(tag);
            break;
        case ZVITAG::ZVITAG_SCALE_UNIT_Z:
            unitsZ = std::get<int32_t>(tag);
            break;
        case ZVITAG::ZVITAG_FILE_NAME:
            m_SceneName = std::get<std::string>(tag);
            break;
        case ZVITAG::ZVITAG_COMPRESSION:
            break;
        }
        }
        catch (const std::bad_variant_access&) {
            // The tag carries a type this case does not expect. The stream is
            // still in sync, so only this one tag is lost.
            SLIDEIO_LOG(WARNING) << "ZVIImageDriver: /Image/Tags/Contents: tag "
                << static_cast<int>(id) << " has an unexpected value type";
        }
    }
    m_res.x = scaleToResolution(scaleX, unitsX);
    m_res.y = scaleToResolution(scaleY, unitsY);
    m_ZSliceRes = scaleToResolution(scaleZ, unitsZ);
}

void ZVIScene::initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices, cv::OutputArray output)
{
    initializeSceneBlock(blockSize, channelIndices, output);
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

