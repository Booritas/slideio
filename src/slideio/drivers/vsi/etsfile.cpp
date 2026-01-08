// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/vsi/etsfile.hpp"
#include "slideio/drivers/vsi/vsistream.hpp"
#include "slideio/drivers/vsi/vsistruct.hpp"
#include "slideio/drivers/vsi/vsitools.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include "slideio/core/tools/endian.hpp"

using namespace slideio;

slideio::vsi::EtsFile::EtsFile(const std::string& filePath) : m_filePath(filePath) {
}

bool vsi::EtsFile::assignVolume(std::list<std::shared_ptr<vsi::Volume>>& volumes) {
    const int minWidth = m_maxCoordinates[0] * m_tileSize.width;
    const int minHeight = m_maxCoordinates[1] * m_tileSize.height;
    const int maxWidth = minWidth + m_tileSize.width;
    const int maxHeight = minHeight + m_tileSize.height;

    for (auto it = volumes.begin(); it != volumes.end(); ++it) {
        const std::shared_ptr<Volume> volume = *it;
        const cv::Size volumeSize = volume->getSize();
        const int volumeWidth = volumeSize.width;
        const int volumeHeight = volumeSize.height;
        if (volumeWidth >= minWidth && volumeWidth <= maxWidth && volumeHeight >= minHeight && volumeHeight <=
            maxHeight) {
            volumes.erase(it);
            setVolume(volume);
            break;
        }
    }
    return m_volume !=nullptr;
}

void vsi::EtsFile::initStruct(TileInfoListPtr& tiles) {

    if (m_volume) {
        m_size.width = m_volume->getSize().width;
        m_size.height = m_volume->getSize().height;
    }

    const int zIndex = m_volume->getDimensionOrder(Dimensions::Z);
    if (zIndex > 1 && zIndex < m_maxCoordinates.size()) {
        m_numZSlices = m_maxCoordinates[m_volume->getDimensionOrder(Dimensions::Z)] + 1;
    }
    const int tIndex = m_volume->getDimensionOrder(Dimensions::T);
    if (tIndex > 1 && tIndex < m_maxCoordinates.size()) {
        m_numTFrames = m_maxCoordinates[m_volume->getDimensionOrder(Dimensions::T)] + 1;
    }
    const int lambdaIndex = m_volume->getDimensionOrder(Dimensions::L);
    if (lambdaIndex > 1 && lambdaIndex < m_maxCoordinates.size()) {
        m_numLambdas = m_maxCoordinates[m_volume->getDimensionOrder(Dimensions::L)] + 1;
    }
    const int channelIndex = m_volume->getDimensionOrder(Dimensions::C);
    if (channelIndex > 1 && channelIndex < m_maxCoordinates.size()) {
        m_numChannels = m_maxCoordinates[m_volume->getDimensionOrder(Dimensions::C)] + 1;
    }

    m_pyramid.init(tiles, m_size, m_tileSize, m_volume.get());

    const int numChannelIndices = m_pyramid.getNumChannelIndices();
    if (numChannelIndices > 1 && numChannelIndices != getNumChannels()) {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: init: Unexpected number of channel indices "
            << numChannelIndices << ". Expected 1 or " << getNumChannels();
    }
}

void slideio::vsi::EtsFile::read(std::list<std::shared_ptr<Volume>>& volumes, std::shared_ptr<std::vector<TileInfo>>& tiles) {
    // Open the file
    m_etsStream = std::make_unique<vsi::VSIStream>(m_filePath);
    vsi::EtsVolumeHeader header = {0};
    m_etsStream->read<vsi::EtsVolumeHeader>(header);
    fromLittleEndianToNative(header);

    if (strncmp((char*)header.magic, "SIS", 3) != 0) {
        RAISE_RUNTIME_ERROR << "VSI driver: invalid ETS file header. Expected first tree bytes: 'SIS', got: "
            << header.magic;
    }
    if (header.headerSize != 64) {
        RAISE_RUNTIME_ERROR << "VSI driver: invalid file header. Expected header size: 64, got: "
            << header.headerSize;
    }
    m_etsStream->setPos(header.additionalHeaderPos);
    ETSAdditionalHeader additionalHeader = {0};
    m_etsStream->read<vsi::ETSAdditionalHeader>(additionalHeader);
	fromLittleEndianToNative(additionalHeader);

    if (strncmp((char*)additionalHeader.magic, "ETS", 3) != 0) {
        RAISE_RUNTIME_ERROR << "VSI driver: invalid ETS file header. Expected first tree bytes: 'ETS', got: "
            << header.magic;
    }
    m_numDimensions = static_cast<int>(header.numDimensions);
    m_dataType = VSITools::toSlideioPixelType(additionalHeader.componentType);
    m_numChannels = static_cast<int>(additionalHeader.componentCount);
    m_colorSpace = static_cast<ColorSpace>(additionalHeader.colorSpace);
    m_compression = VSITools::toSlideioCompression(static_cast<vsi::Compression>(additionalHeader.format));
    m_compressionQuality = static_cast<int>(additionalHeader.quality);
    m_tileSize.width = static_cast<int>(additionalHeader.sizeX);
    m_tileSize.height = static_cast<int>(additionalHeader.sizeY);
    m_numZSlices = static_cast<int>(additionalHeader.sizeZ);
    std::memcpy(m_pixelInfoHints, additionalHeader.pixInfoHints, sizeof(m_pixelInfoHints));
    std::memcpy(m_backgroundColor, additionalHeader.background, sizeof(m_backgroundColor));
    m_usePyramid = additionalHeader.usePyramid != 0;

    m_etsStream->setPos(header.usedChunksPos);
    tiles->resize(header.numUsedChunks);
    m_maxCoordinates.resize(m_numDimensions);
    for (uint chunk = 0; chunk < header.numUsedChunks; ++chunk) {
        TileInfo& tileInfo = tiles->at(chunk);
        m_etsStream->skipBytes(4);
        tileInfo.coordinates.resize(m_numDimensions);
        for (int i = 0; i < m_numDimensions; ++i) {
            tileInfo.coordinates[i] = m_etsStream->readValue<int32_t>();
			tileInfo.coordinates[i] = Endian::fromLittleEndianToNative(tileInfo.coordinates[i]);
            m_maxCoordinates[i] = std::max(m_maxCoordinates[i], tileInfo.coordinates[i]);
        }
        tileInfo.offset = m_etsStream->readValue<int64_t>();
		tileInfo.offset = Endian::fromLittleEndianToNative(tileInfo.offset);
        tileInfo.size = m_etsStream->readValue<uint32_t>();
		tileInfo.size = Endian::fromLittleEndianToNative(tileInfo.size);
        m_etsStream->skipBytes(4);
    }

    const int minWidth = m_maxCoordinates[0] * m_tileSize.width;
    const int minHeight = m_maxCoordinates[1] * m_tileSize.height;
    const int maxWidth = minWidth + m_tileSize.width;
    const int maxHeight = minHeight + m_tileSize.height;

    m_sizeWithCompleteTiles = cv::Size(maxWidth, maxHeight);

}

void vsi::EtsFile::readTilePart(const vsi::TileInfo& tileInfo, cv::OutputArray tileRaster) {
    const int64_t offset = tileInfo.offset;
    const uint32_t tileCompressedSize = tileInfo.size;
    const int ds = CVTools::cvGetDataTypeSize(m_dataType);
    m_etsStream->setPos(offset);
    m_buffer.resize(tileCompressedSize);
    m_etsStream->readBytes(m_buffer.data(), static_cast<int>(m_buffer.size()));
    tileRaster.create(m_tileSize, CV_MAKETYPE(CVTools::cvTypeFromDataType(m_dataType), 1));
    if (m_compression == slideio::Compression::Uncompressed) {
        const int tileSize = m_tileSize.width * m_tileSize.height * ds;
        std::memcpy(tileRaster.getMat().data, m_buffer.data(), tileSize);
    }
    else if (m_compression == slideio::Compression::Jpeg) {
        ImageTools::decodeJpegStream(m_buffer.data(), m_buffer.size(), tileRaster);
    }
    else if (m_compression == slideio::Compression::Jpeg2000) {
        ImageTools::decodeJp2KStream(m_buffer.data(), m_buffer.size(), tileRaster);
    }
    else {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: readTile: Compression " << static_cast<int>(m_compression)
            << " is not supported";
    }
}

void vsi::EtsFile::readTile(int levelIndex,
                            int tileIndex,
                            const std::vector<int>& channelIndices,
                            int zSlice,
                            int tFrame,
                            cv::OutputArray output) {
    if (levelIndex < 0 || levelIndex >= m_pyramid.getNumLevels()) {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: readTile: Pyramid level "
            << levelIndex << " is out of range (0 - " << m_pyramid.getNumLevels() << " )";
    }
    const PyramidLevel& pyramidLevel = m_pyramid.getLevel(levelIndex);

    if (tileIndex < 0 || tileIndex >= pyramidLevel.getNumTiles()) {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: readTile: Tile index "
            << tileIndex << " is out of range (0 - " << pyramidLevel.getNumTiles() << " )";
    }
    const int numChannelIndices = m_pyramid.getNumChannelIndices();

    if (numChannelIndices > 1) {
        std::list<int> channelList(channelIndices.begin(), channelIndices.end());
        if (channelList.empty()) {
            for (int i = 0; i < getNumChannels(); ++i) {
                channelList.push_back(i);
            }
        }
        std::vector<cv::Mat> channelRasters(channelList.size());
        int rasterIndex = 0;
        for (const int channelIndex : channelList) {
            if (channelIndex < 0 || channelIndex >= getNumChannels()) {
                RAISE_RUNTIME_ERROR << "VSIImageDriver: readTile: Channel index "
                    << channelIndex << " is out of range (0 - " << numChannelIndices << " )";
            }
            const TileInfo& tileInfo = pyramidLevel.getTile(tileIndex, channelIndex, zSlice, tFrame);
            readTilePart(tileInfo, channelRasters[rasterIndex++]);
        }
        if (channelRasters.size() == 1) {
            channelRasters[0].copyTo(output);
        }
        else {
            cv::merge(channelRasters, output);
        }
    }
    else {
        cv::Mat tileRaster;
        const TileInfo& tileInfo = pyramidLevel.getTile(tileIndex, 0, zSlice, tFrame);
        readTilePart(tileInfo, tileRaster);
        Tools::extractChannels(tileRaster, channelIndices, output);
    }
}
