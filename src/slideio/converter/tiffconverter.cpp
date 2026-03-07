// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "tiffconverter.hpp"
#include "converterparameters.hpp"
#include "convertertools.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/imagetools/tiffmessagehandler.hpp"
#include "slideio/imagetools/libtiff.hpp"
#include "slideio/core/tools/color_tools.hpp"
#include <algorithm>
#include <filesystem>
#include <tinyxml2.h>
#include <thread>
#include <mutex>
#include <future>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <map>

#include "slideio/base/log.hpp"
#include "slideio/core/tools/boundedqueue.hpp"
#include "slideio/slideio/slideio.hpp"

using namespace slideio;
using namespace slideio::converter;

const TiffPageStructure& TiffConverter::getTiffPage(int index) const {
    if (index >= m_pages.size()) {
        RAISE_RUNTIME_ERROR << "Converter: TIFF page index out of range!";
    }
    return m_pages[index];
}

DataType TiffConverter::getChannelRangeDataType(const Range& range) const {
    makeSureValid();
    const int numSceneChannels = m_scene->getNumChannels();
    if (range.end > numSceneChannels) {
        RAISE_RUNTIME_ERROR << "Converter: channel range exceeds number of scene channels!";
    }
    const DataType dataType = m_scene->getChannelDataType(range.start);
    for (int channel = range.start + 1; channel < range.end; ++channel) {
        if (m_scene->getChannelDataType(channel) != dataType) {
            return DataType::DT_Unknown;
        }
    }
    return dataType;
}

int TiffConverter::computeChannelChunk(int firstChannel, const std::shared_ptr<CVScene>& scene) const {
    makeSureValid();
    std::shared_ptr<const EncodeParameters> encoding = m_parameters.getEncodeParameters();
    Range channelRange = m_parameters.getChannelRange();
    Compression compression = encoding->getCompression();
    const int numSceneChannels = m_scene->getNumChannels();
    int channelChunkSize;
    const bool canGroupChannelsBy3 = (numSceneChannels == 3)
        && (firstChannel == 0)
        && (channelRange.size() == 3)
        && (scene->getNumChannels() == 3);
    if (compression == Compression::Jpeg) {
        if (canGroupChannelsBy3 && getChannelRangeDataType(channelRange) == DataType::DT_Byte) {
            channelChunkSize = 3;
        }
        else if (m_scene->getChannelDataType(firstChannel) == DataType::DT_Byte) {
            channelChunkSize = 1;
        }
        else {
            RAISE_RUNTIME_ERROR << "Converter: JPEG compression supports only 8-bit channels.";
        }
    }
    else if (compression == Compression::Jpeg2000) {
        if (canGroupChannelsBy3) {
            channelChunkSize = 3;
        }
        else {
            channelChunkSize = 1;
        }
    }
    else {
        RAISE_RUNTIME_ERROR << "Converter: unsupported compression type: " << compression;
    }
    return channelChunkSize;
}

std::string TiffConverter::createSVSImageDescription() const {
    auto rect = m_scene->getRect();
    std::shared_ptr<const TIFFContainerParameters> tiffParams = std::static_pointer_cast<const TIFFContainerParameters>(
        m_parameters.getContainerParameters());
    std::stringstream buff;
    buff << "SlideIO Library 2.0\n";
    buff << rect.width << "x" << rect.height;
    buff << "(" << tiffParams->getTileWidth() << "x" << tiffParams->getTileHeight() << ") ";
    if (m_parameters.getEncoding() == Compression::Jpeg) {
        std::shared_ptr<const JpegEncodeParameters> jpegParams = std::static_pointer_cast<const JpegEncodeParameters>(
            m_parameters.getEncodeParameters());
        buff << "JPEG/RGB " << "Q=" << jpegParams->getQuality();
    }
    else if (m_parameters.getEncoding() == Compression::Jpeg2000) {
        buff << "J2K";
    }
    buff << "\n";
    double magn = m_scene->getMagnification();
    Resolution resolution = m_scene->getResolution();
    if (resolution.x > 0) {
        buff << "|MPP = " << resolution.x * 1.e6;
    }
    if (magn > 0) {
        buff << "|AppMag = " << magn;
    }

    std::string filePath = m_scene->getFilePath();
    std::filesystem::path path(filePath);
    buff << "|Filename = " << path.stem().string();
    buff << "|" << SVSDateString() << "|" << SVSTimeString();

    return buff.str();
}

std::string TiffConverter::createImageDescriptionTag() const {
    makeSureValid();
    if (m_parameters.getFormat() == ImageFormat::SVS) {
        return createSVSImageDescription();
    }
    else if (m_parameters.getFormat() == ImageFormat::OME_TIFF) {
        return createOMETiffDescription();
    }
    else {
        RAISE_RUNTIME_ERROR << "Converter: Unrecognized target image format: " << static_cast<int>(m_parameters.
            getFormat());
    }
}

std::string TiffConverter::createOMETiffDescription() const {
    makeSureValid();
    tinyxml2::XMLDocument doc;
    auto* ome = doc.NewElement("OME");
    ome->SetAttribute("xmlns", "http://www.openmicroscopy.org/Schemas/OME/2016-06");
    ome->SetAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
    ome->SetAttribute("xsi:schemaLocation",
                      "http://www.openmicroscopy.org/Schemas/OME/2016-06 http://www.openmicroscopy.org/Schemas/OME/2016-06/ome.xsd");
    doc.InsertFirstChild(ome);
    bool interleaved = false;
    auto rect = m_parameters.getRect();
    const int sizeX = rect.width;
    const int sizeY = rect.height;
    const int numChannels = m_parameters.getChannelRange().size();
    const int numZSlices = m_parameters.getSliceRange().size();
    const int numTFrames = m_parameters.getTFrameRange().size();
    double magnification = m_scene->getMagnification();
    if (magnification > 0) {
        auto* instrument = doc.NewElement("Instrument");
        instrument->SetAttribute("ID", "Instrument:0");
        auto* objective = doc.NewElement("Objective");
        objective->SetAttribute("ID", "Objective:0");
        objective->SetAttribute("NominalMagnification", magnification);
        instrument->InsertEndChild(objective);
        ome->InsertEndChild(instrument);
    }
    auto* image = doc.NewElement("Image");
    image->SetAttribute("ID", "Image:0");
    std::string name = m_scene->getName();
    if (!name.empty()) image->SetAttribute("Name", name.c_str());
    ome->InsertEndChild(image);

    auto* pixels = doc.NewElement("Pixels");

    // Channels
    if (!m_pages.empty()) {
        const auto& firstPage = m_pages.front();
        const auto& channelRange = firstPage.getChannelRange();
        if ((channelRange.size() == 3)
            && (m_scene->getChannelDataType(channelRange.start) == DataType::DT_Byte)) {
            interleaved = true;
        }
    }

    int id = 0;
    const auto& sceneChannelRange = m_parameters.getChannelRange();
    for (int channel = sceneChannelRange.start; channel < sceneChannelRange.end; ++channel) {
        std::string idAttr = std::string("Channel:0:") + std::to_string(id++);
        auto* xmlChannel = doc.NewElement("Channel");
        for (int attIndex = 0; attIndex < (int)m_scene->getNumChannelAttributes(); ++attIndex) {
            std::string attrValue = m_scene->getChannelAttributeValue(channel, attIndex);
            const std::string& attrName = m_scene->getChannelAttributeName(attIndex);
            if (attrName == "Color" && ColorTools::detectHexColorFormat(attrValue) != HexColorFormat::UNKNOWN) {
                attrValue = ColorTools::hexToInt32String(attrValue);
            }
            xmlChannel->SetAttribute(attrName.c_str(), attrValue.c_str());
        }
        xmlChannel->SetAttribute("ID", idAttr.c_str());
        xmlChannel->SetAttribute("SamplesPerPixel", 1);
        const std::string& channelName = m_scene->getChannelName(channel);
        if (!channelName.empty())
            xmlChannel->SetAttribute("Name", channelName.c_str());
        pixels->InsertEndChild(xmlChannel);
    }

    pixels->SetAttribute("ID", "Pixels:0");
    std::string order = "XYCZT";
    pixels->SetAttribute("DimensionOrder", order.c_str());
    pixels->SetAttribute("SizeX", sizeX);
    pixels->SetAttribute("SizeY", sizeY);
    pixels->SetAttribute("SizeZ", std::max(1, numZSlices));
    pixels->SetAttribute("SizeC", std::max(1, numChannels));
    pixels->SetAttribute("SizeT", std::max(1, numTFrames));
    pixels->SetAttribute("BigEndian", false);
    pixels->SetAttribute("Interleaved", interleaved);

    // DataType mapping
    auto dt = m_scene->getChannelDataType(0);
    const char* typeStr = "uint8";
    switch (dt) {
    case DataType::DT_Byte: typeStr = "uint8";
        break;
    case DataType::DT_UInt16: typeStr = "uint16";
        break;
    case DataType::DT_Int16: typeStr = "int16";
        break;
    case DataType::DT_Int32: typeStr = "int32";
        break;
    case DataType::DT_Float32: typeStr = "float";
        break;
    case DataType::DT_Float64: typeStr = "double";
        break;
    default: typeStr = "uint8";
        break;
    }
    pixels->SetAttribute("Type", typeStr);

    // Physical sizes
    Resolution res = m_scene->getResolution();
    if (res.x > 0) {
        pixels->SetAttribute("PhysicalSizeX", 1000 * res.x);
        pixels->SetAttribute("PhysicalSizeXUnit", "mm");
    }
    if (res.y > 0) {
        pixels->SetAttribute("PhysicalSizeY", 1000 * res.y);
        pixels->SetAttribute("PhysicalSizeYUnit", "mm");
    }
    double resZ = m_scene->getZSliceResolution();
    double resT = m_scene->getTFrameResolution();
    if (resZ > 0) {
        pixels->SetAttribute("PhysicalSizeZ", resZ * 1000);
        pixels->SetAttribute("PhysicalSizeZUnit", "mm");
    }
    if (resT > 0) {
        pixels->SetAttribute("PhysicalSizeT", resT);
    }
    image->InsertEndChild(pixels);

    std::string fileName;
    if (!m_filePath.empty()) {
        std::filesystem::path p(m_filePath);
        fileName = p.filename().string();
    }
    // TiffData elements
    int ifd = 0;
    int channel = 0;
    int slice = 0;
    int frame = 0;

    std::string UUID = Tools::randomUUID();

    for (const auto& page : m_pages) {
        auto* tiffData = doc.NewElement("TiffData");
        cv::Range sliceRange = page.getZSliceRange();
        cv::Range channelRange = page.getChannelRange();
        cv::Range frameRange = page.getTFrameRange();
        tiffData->SetAttribute("IFD", ifd++);
        tiffData->SetAttribute("FirstC", channel);
        tiffData->SetAttribute("FirstZ", slice);
        tiffData->SetAttribute("FirstT", frame);
        tiffData->SetAttribute("PlaneCount", page.getPlaneCount());
        auto* uidElem = doc.NewElement("UUID");
        uidElem->SetAttribute("FileName", fileName.c_str());
        uidElem->SetText(UUID.c_str());
        tiffData->InsertEndChild(uidElem);
        pixels->InsertEndChild(tiffData);
        channel += channelRange.size();
        if (channel >= numChannels) {
            channel = 0;
            slice += sliceRange.size();
            if (slice >= numZSlices) {
                channel = 0;
                slice = 0;
                frame += frameRange.size();
            }
        }
    }
    tinyxml2::XMLPrinter printer;
    doc.Print(&printer);
    return std::string(printer.CStr());
}

void TiffConverter::createFileLayout(const std::shared_ptr<CVScene>& scene, const ConverterParameters& parameters) {
    if (!scene) {
        RAISE_RUNTIME_ERROR << "Converter: invalid scene provided";
    }
    if (!parameters.isValid()) {
        RAISE_RUNTIME_ERROR << "Converter: invalid converter parameters";
    }
    m_scene = scene;
    m_parameters = parameters;
    m_parameters.updateNotDefinedParameters(scene);
    m_pages.clear();
    m_file.reset();
    m_filePath.clear();
    m_totalTiles = 0;
    m_currentTile = 0;
    m_lastProgress = 0;
    if (parameters.getContainerType() != Container::TIFF_CONTAINER) {
        RAISE_RUNTIME_ERROR << "Converter: TIFF structure can be created only for TIFF container parameters!";
    }
    ImageFormat format = parameters.getFormat();
    if (format != ImageFormat::SVS && format != ImageFormat::OME_TIFF) {
        RAISE_RUNTIME_ERROR << "Converter: Unrecognized target image format: " << static_cast<int>(format);
    }
    makeSureValid();
    checkEncodingRequirements();
    checkContainerRequirements();
    computeCropRect();
    const Range channelRange = m_parameters.getChannelRange();
    const Range frameRange = m_parameters.getTFrameRange();
    const Range sliceRange = m_parameters.getSliceRange();
    if (format == ImageFormat::SVS) {
        if (frameRange.size() != 1 || sliceRange.size() != 1) {
            RAISE_RUNTIME_ERROR << "Converter: SVS format supports only single time-frame and single z-slice images!";
        }
    }
    std::shared_ptr<const TIFFContainerParameters> tiffParams =
        std::static_pointer_cast<const TIFFContainerParameters>(m_parameters.getContainerParameters());
    cv::Size tileSize(tiffParams->getTileWidth(), tiffParams->getTileHeight());
    m_totalTiles = 0;

    int channelChunkSize = scene->getNumChannels();
    if (format == OME_TIFF) {
        channelChunkSize = computeChannelChunk(channelRange.start, scene);
    }

    int numZoomLevels = tiffParams->getNumZoomLevels();
    for (int frame = frameRange.start; frame < frameRange.end; ++frame) {
        for (int slice = sliceRange.start; slice < sliceRange.end; ++slice) {
            for (int channel = channelRange.start; channel < channelRange.end; channel += channelChunkSize) {
                const int pageIndex = static_cast<int>(m_pages.size());
                TiffPageStructure& page = appendPage();
                Rect imageRect = m_cropRect;
                m_totalTiles += ConverterTools::computeNumTiles(m_cropRect.size(), tileSize);
                page.setChannelRange(cv::Range(channel, channel + channelChunkSize));
                page.setZSliceRange(cv::Range(slice, slice + 1));
                page.setTFrameRange(cv::Range(frame, frame + 1));
                page.setZoomLevelRange(cv::Range(0, 1));
                int planeCount = page.getZSliceRange().size() * page.getTFrameRange().size();
                page.setPlaneCount(planeCount);
                for (int zoomLevel = 1; zoomLevel < numZoomLevels; ++zoomLevel) {
                    cv::Rect zoomLevelRect = ConverterTools::computeZoomLevelRect(imageRect, tileSize, zoomLevel);
                    m_totalTiles += ConverterTools::computeNumTiles(zoomLevelRect.size(), tileSize);
                    if (format == ImageFormat::OME_TIFF) {
                        TiffDirectoryStructure& dir = m_pages[pageIndex].appendSubDirectory();
                        dir = static_cast<const TiffDirectoryStructure&>(m_pages[pageIndex]);
                        dir.setZoomLevelRange(cv::Range(zoomLevel, zoomLevel + 1));
                    }
                    else if (format == ImageFormat::SVS) {
                        TiffPageStructure& dir = appendPage();
                        dir = static_cast<const TiffPageStructure&>(m_pages[pageIndex]);
                        dir.setZoomLevelRange(cv::Range(zoomLevel, zoomLevel + 1));
                    }
                }
            }
        }
    }

    bool hasChannelNames = false;
    for (int channel = channelRange.start; channel < channelRange.end; ++channel) {
        std::string name = m_scene->getChannelName(channel);
        if (!name.empty()) {
            hasChannelNames = true;
            break;
        }
    }
}


void TiffConverter::computeCropRect() {
    m_cropRect = m_scene->getRect();
    m_cropRect.x = m_cropRect.y = 0;
    if (!m_parameters.getRect().valid()) {
        RAISE_RUNTIME_ERROR << "Converter: Invalid rectangle for the scene converter!";
    }
    const cv::Rect sceneRect = m_scene->getRect();
    const auto& block = m_parameters.getRect();
    if (block.x + block.width > sceneRect.width) {
        RAISE_RUNTIME_ERROR << "Converter: Crop rectangle exceeds scene width!";
    }
    if (block.y + block.height > sceneRect.height) {
        RAISE_RUNTIME_ERROR << "Converter: Crop rectangle exceeds scene height!";
    }
    m_cropRect.x = block.x;
    m_cropRect.y = block.y;
    m_cropRect.width = block.width;
    m_cropRect.height = block.height;
}

TiffDirectory TiffConverter::setUpDirectory(const TiffDirectoryStructure& page) {
    TiffDirectory dir;
    const int zoomLevel = page.getZoomLevelRange().start;

    std::shared_ptr<const TIFFContainerParameters> tiffParams = std::static_pointer_cast<const TIFFContainerParameters>(
        m_parameters.getContainerParameters());
    cv::Size tileSize(tiffParams->getTileWidth(), tiffParams->getTileHeight());
    cv::Size levelImageSize = ConverterTools::scaleSize(m_cropRect.size(), zoomLevel);

    dir.tiled = true;
    dir.channels = page.getChannelRange().size();
    dir.dataType = m_scene->getChannelDataType(page.getChannelRange().start);
    if (m_parameters.getEncoding() == Compression::Jpeg || m_parameters.getEncoding() == Compression::Jpeg2000) {
        dir.slideioCompression = m_parameters.getEncoding();
    }
    else {
        RAISE_RUNTIME_ERROR << "Converter: Unexpected compression type: " << (int)m_parameters.getEncoding();
    }
    dir.width = levelImageSize.width;
    dir.height = levelImageSize.height;
    dir.tileWidth = tileSize.width;
    dir.tileHeight = tileSize.height;
    if (m_parameters.getEncoding() == Compression::Jpeg) {
        std::shared_ptr<const JpegEncodeParameters> jpegParams = std::static_pointer_cast<const JpegEncodeParameters>(
            m_parameters.getEncodeParameters());
        dir.compressionQuality = jpegParams->getQuality();
    }
    dir.description = page.getDescription();
    dir.res = m_scene->getResolution();
    dir.software = std::string("SlideIO Library ") + getVersion();
    dir.subFileType = 0; // FILETYPE_PAGE;
    return dir;
}

void TiffConverter::writeDirectoryData(TiffDirectory& dir, const TiffDirectoryStructure& page, const std::function<void(int)>& cb, int param) {
    if (page.getZoomLevelRange().size() != 1) {
        RAISE_RUNTIME_ERROR << "Converter: Invalid zoom level range in page! Expected: 1, received: " << page.
            getZoomLevelRange().size();
    }
    if (m_parameters.getEncodeParameters()->getCompression() == Compression::Jpeg2000 || param != 1) {
        writeDirectoryDataMT(dir, page, cb, param);
    }
    else {
        writeDirectoryDataST(dir, page, cb, param);
    }
}

void TiffConverter::writeDirectoryDataST(TiffDirectory& dir, const TiffDirectoryStructure& page, const std::function<void(int)>& cb, int tileBatchSize) {
    if (page.getZoomLevelRange().size() != 1) {
        RAISE_RUNTIME_ERROR << "Converter: Invalid zoom level range in page! Expected: 1, received: " << page.getZoomLevelRange().size();
    }
    int zoomLevel = page.getZoomLevelRange().start;
    cv::Size tileSize = cv::Size(dir.tileWidth, dir.tileHeight);
    cv::Size sceneTileSize = ConverterTools::scaleSize(tileSize, zoomLevel, false);
    std::vector<uint8_t> buffer;
    if (m_parameters.getEncoding() == Compression::Jpeg2000) {
        int dataSize = tileSize.width * tileSize.height * m_scene->getNumChannels() * Tools::dataTypeSize(m_scene->getChannelDataType(0));
        buffer.resize(dataSize);
    }
    cv::Mat block;
    std::shared_ptr<const EncodeParameters> encoding = m_parameters.getEncodeParameters();
    const int xEnd = m_cropRect.x + m_cropRect.width;
    const int yEnd = m_cropRect.y + m_cropRect.height;
    const int slice = page.getZSliceRange().start;
    const int frame = page.getTFrameRange().start;
    std::vector<int> channels;
    channels.reserve(dir.channels);
    for (int channel = 0; channel < dir.channels; ++channel) {
        channels.push_back(page.getChannelRange().start + channel);
    }
    const int batchWidth = tileBatchSize * sceneTileSize.width;
    for (int y = m_cropRect.y; y < yEnd; y += sceneTileSize.height) {
        for (int x = m_cropRect.x; x < xEnd; x += batchWidth) {
            int numTiles = 1 + (xEnd - x - 1) / sceneTileSize.width;
            numTiles = std::min(numTiles, tileBatchSize);
            numTiles = std::max(1, numTiles);
            const int blockWidth = numTiles * sceneTileSize.width;
            cv::Rect blockRect(x, y, blockWidth, sceneTileSize.height);
            auto readStart = std::chrono::high_resolution_clock::now();
            ConverterTools::readTile(m_scene, channels, zoomLevel, blockRect, slice, frame, block);
            auto readEnd = std::chrono::high_resolution_clock::now();
            m_readTime += std::chrono::duration_cast<std::chrono::microseconds>(readEnd - readStart).count();
            if (block.rows != tileSize.height || block.cols != tileSize.width * numTiles) {
                RAISE_RUNTIME_ERROR << "Converter: Unexpected tile size ("
                    << block.cols << ","
                    << block.rows << "). Expected tile size: ("
                    << tileSize.width << ","
                    << tileSize.height << ").";
            }
            blockRect.x -= m_cropRect.x;
            blockRect.y -= m_cropRect.y;
            cv::Rect zoomLevelRect = ConverterTools::scaleRect(blockRect, zoomLevel, true);
            for (int blockTile = 0; blockTile < numTiles; ++blockTile) {
                cv::Rect tileRect(blockTile * tileSize.width, 0, tileSize.width, tileSize.height);
                cv::Mat tile;
                block(tileRect).copyTo(tile);
                const int tileWritePosX = zoomLevelRect.x + blockTile * tileSize.width;
                auto writeStart = std::chrono::high_resolution_clock::now();
                m_file->writeTile(tileWritePosX, zoomLevelRect.y, dir.slideioCompression, *encoding, tile, buffer.data(), (int)buffer.size());
                auto writeEnd = std::chrono::high_resolution_clock::now();
                m_writeTime += std::chrono::duration_cast<std::chrono::microseconds>(writeEnd - writeStart).count();
                m_currentTile++;
                if (cb) {
                    double proc = 100. * (double)m_currentTile / (double)m_totalTiles;
                    if (const int lproc = std::lround(proc); lproc != m_lastProgress) {
                        cb(lproc);
                        m_lastProgress = lproc;
                    }
                }
            }
        }
    }
}

void TiffConverter::readTiles(TiffDirectory& dir, const TiffDirectoryStructure& page, BoundedQueue<Tile>& inputQueue, int tileBatchSize) {
    int zoomLevel = page.getZoomLevelRange().start;
    cv::Size tileSize = cv::Size(dir.tileWidth, dir.tileHeight);
    cv::Size sceneTileSize = ConverterTools::scaleSize(tileSize, zoomLevel, false);
    std::shared_ptr<const EncodeParameters> encoding = m_parameters.getEncodeParameters();
    const int xEnd = m_cropRect.x + m_cropRect.width;
    const int yEnd = m_cropRect.y + m_cropRect.height;
    const int slice = page.getZSliceRange().start;
    const int frame = page.getTFrameRange().start;
    std::vector<int> channels;
    channels.reserve(dir.channels);
    for (int channel = 0; channel < dir.channels; ++channel) {
        channels.push_back(page.getChannelRange().start + channel);
    }
    std::exception_ptr readerException;
	try {
		int iTile = 0;
		const int batchWidth = tileBatchSize * sceneTileSize.width;
		cv::Mat block;
		bool done = false;
		for (int y = m_cropRect.y; y < yEnd && !done; y += sceneTileSize.height) {
			for (int x = m_cropRect.x; x < xEnd && !done; x += batchWidth) {
                int numTiles = 1 + (xEnd - x - 1) / sceneTileSize.width;
                numTiles = std::min(numTiles, tileBatchSize);
                numTiles = std::max(1, numTiles);
                const int blockWidth = numTiles * sceneTileSize.width;
                cv::Rect blockRect(x, y, blockWidth, sceneTileSize.height);
                auto readStart = std::chrono::high_resolution_clock::now();
                ConverterTools::readTile(m_scene, channels, zoomLevel, blockRect, slice, frame, block);
                auto readEnd = std::chrono::high_resolution_clock::now();
                m_readTime += std::chrono::duration_cast<std::chrono::microseconds>(readEnd - readStart).count();
                if (block.rows != tileSize.height || block.cols != tileSize.width * numTiles) {
                    RAISE_RUNTIME_ERROR << "Converter: Unexpected tile size ("
                        << block.cols << ","
                        << block.rows << "). Expected tile size: ("
                        << tileSize.width << ","
                        << tileSize.height << ").";
                }
                blockRect.x -= m_cropRect.x;
                blockRect.y -= m_cropRect.y;
                cv::Rect zoomLevelRect = ConverterTools::scaleRect(blockRect, zoomLevel, true);
                for (int blockTile = 0; blockTile < numTiles; ++blockTile) {
                    cv::Rect tileRect(blockTile * tileSize.width, 0, tileSize.width, tileSize.height);
                    Tile tileInfo;
                    cv::Mat& tile = tileInfo.raster;
                    tileInfo.sequenceId = iTile++;
                    block(tileRect).copyTo(tile);
                    const int tileWritePosX = zoomLevelRect.x + blockTile * tileSize.width;
                    tileInfo.location = cv::Point2i(tileWritePosX, zoomLevelRect.y);
                    if (!inputQueue.push(std::move(tileInfo))) {
						done = true;
                        break;
                    }
                }
            }
        }
    }
	catch (const std::exception& e) {
		readerException = std::current_exception();
		SLIDEIO_LOG(ERROR) << "Converter: Exception in tile reader thread: " << e.what();
	}
	catch (...) {
		readerException = std::current_exception();
		SLIDEIO_LOG(ERROR) << "Converter: Unknown exception in tile reader thread";
	}
    inputQueue.setDone();
}

std::vector<uint8_t> TiffConverter::encodeTile(const cv::Mat& tileRaster) {
    Compression compression = m_parameters.getEncodeParameters()->getCompression();
    if (compression != Compression::Jpeg2000) {
        RAISE_RUNTIME_ERROR << "Unsupported compression type for multi-threaded compression.";
    }
    std::vector<uint8_t> buff;
    const size_t dataSize = tileRaster.total() * tileRaster.elemSize();
    buff.resize(dataSize);
    std::shared_ptr<JP2KEncodeParameters> jp2param =
        std::static_pointer_cast<JP2KEncodeParameters>(m_parameters.getEncodeParameters());
    const int encodedSize = ImageTools::encodeJp2KStream(tileRaster, buff.data(), static_cast<int>(buff.size()), *jp2param);
    if (dataSize <= 0) {
        RAISE_RUNTIME_ERROR << "JPEG 2000 Encoding failed";
    }
    buff.resize(encodedSize);
    return buff;
}

void TiffConverter::encodeTiles(BoundedQueue<Tile>& inputQueue, BoundedQueue<EncodedTile>& outputQueue,
    std::atomic<size_t>& activeEncoders, std::exception_ptr& encoderException, std::mutex& encoderExMutex) {
    try {
        while (std::optional<Tile> tile = inputQueue.pop()) {
            EncodedTile encoded;
            encoded.sequenceId = tile->sequenceId;
            encoded.location = tile->location;
            encoded.encodedData = encodeTile(tile->raster);
            if (!outputQueue.push(std::move(encoded))) {
                break;
            }
        }
    }
    catch (std::exception& e) {
        {
            std::unique_lock lock(encoderExMutex);
            if (!encoderException)
                encoderException = std::current_exception();
        }
        inputQueue.setDone();
        SLIDEIO_LOG(ERROR) << "Converter: Exception in tile encoder thread: " << e.what();
    }
    catch (...) {
        {
            std::unique_lock lock(encoderExMutex);
            if (!encoderException)
                encoderException = std::current_exception();
        }
        inputQueue.setDone();
        SLIDEIO_LOG(ERROR) << "Converter: Unknown exception in tile encoder thread.";
    }
    if (--activeEncoders == 0)
        outputQueue.setDone();
}

void TiffConverter::writeTile(const EncodedTile& tile) const {
    const cv::Point2i& loc = tile.location;
    const std::vector<uint8_t>& buffer = tile.encodedData;
    m_file->writeRawTile(loc.x, loc.y, buffer.data(), static_cast<int>(buffer.size()));
}

void TiffConverter::writeTiles(BoundedQueue<Tile>& inputQueue, BoundedQueue<EncodedTile>& outputQueue, const std::function<void(int)>& cb) {
    std::map<size_t, EncodedTile> reorderBuffer; // Holds out-of-order tiles
    std::exception_ptr writerException;
    size_t nextExpected = 0;

    try {
        while (auto encoded = outputQueue.pop()) {
            reorderBuffer.emplace(encoded->sequenceId, std::move(*encoded));
            // Flush all consecutive tiles that are ready
            while (reorderBuffer.count(nextExpected)) {
                writeTile(reorderBuffer.at(nextExpected));  // Fast I/O
                reorderBuffer.erase(nextExpected);
                m_currentTile++;
                if (cb) {
                    double proc = 100. * (double)m_currentTile / (double)m_totalTiles;
                    if (const int lproc = std::lround(proc); lproc != m_lastProgress) {
                        cb(lproc);
                        m_lastProgress = lproc;
                    }
                }
                ++nextExpected;
            }
        }
    }
    catch (std::exception& e) {
        writerException = std::current_exception();
        outputQueue.setDone();  // Wake any encoders blocked on push()
        inputQueue.setDone();   // Wake reader if blocked on push(), stop encoders
        SLIDEIO_LOG(ERROR) << "Converter: Exception in tile encoder thread: " << e.what();
    }
    catch (...) {
        writerException = std::current_exception();
        outputQueue.setDone();  // Wake any encoders blocked on push()
        inputQueue.setDone();   // Wake reader if blocked on push(), stop encoders
    }
}

void TiffConverter::writeDirectoryDataMT(TiffDirectory& dir, const TiffDirectoryStructure& page,
                                         const std::function<void(int)>& cb, int tileBatchSize, int numEncoderThreads) {
    if (numEncoderThreads <= 0)
        numEncoderThreads = std::thread::hardware_concurrency();
    const size_t QUEUE_DEPTH = numEncoderThreads * 2; // Bound memory usage

    BoundedQueue<Tile> inputQueue(QUEUE_DEPTH);
    BoundedQueue<EncodedTile> outputQueue(QUEUE_DEPTH);
    // --- Stage 1: Reader (single thread) ---
    std::thread reader(&TiffConverter::readTiles, this, std::ref(dir), std::ref(page), std::ref(inputQueue), tileBatchSize);

    // --- Stage 2: Encoders (thread pool) ---
    std::vector<std::thread> encoders;
    std::atomic<size_t> activeEncoders{static_cast<size_t>(numEncoderThreads)};
    std::exception_ptr encoderException;
    std::mutex         encoderExMutex;

    for (size_t i = 0; i < numEncoderThreads; ++i) {
        encoders.emplace_back(&TiffConverter::encodeTiles, this, std::ref(inputQueue), std::ref(outputQueue),
            std::ref(activeEncoders), std::ref(encoderException), std::ref(encoderExMutex));
    }

    // --- Stage 3: Writer (single thread, ordered) ---
    std::thread writer(&TiffConverter::writeTiles, this, std::ref(inputQueue), std::ref(outputQueue), std::ref(cb));

    reader.join();
    for (auto& e : encoders) {
        e.join();
    }
    writer.join();
}


void TiffConverter::createTiff(const std::string& filePath, const std::function<void(int)>& cb, int tileBatchSize) {
    TIFFMessageHandler mh;
    m_currentTile = 0;
    m_file.reset(new TIFFKeeper(filePath, false));
    m_filePath = filePath;
    std::string description = createImageDescriptionTag();
    if (!m_pages.empty()) {
        m_pages.front().setDescription(description);
    }
    for (int pageIndex = 0; pageIndex < getNumTiffPages(); ++pageIndex) {
        const TiffPageStructure& page = getTiffPage(pageIndex);
        TiffDirectory dir = setUpDirectory(page);
        m_file->setTags(dir);
        const int numSubDirs = page.getNumSubDirectories();
        if (numSubDirs > 0) {
            m_file->initSubDirs(numSubDirs);
        }
        writeDirectoryData(dir, page, cb, tileBatchSize);
        m_file->writeDirectory();
        const int numSubdirs = page.getNumSubDirectories();
        for (int subDirIndex = 0; subDirIndex < numSubdirs; ++subDirIndex) {
            const TiffDirectoryStructure& dirSpec = page.getSubDirectory(subDirIndex);
            TiffDirectory subDir = setUpDirectory(dirSpec);
            subDir.subFileType = FILETYPE_REDUCEDIMAGE;
            m_file->setTags(subDir);
            writeDirectoryData(subDir, dirSpec, cb, tileBatchSize);
            m_file->writeDirectory();
        }
    }
}


void TiffConverter::makeSureValid() const {
    if (m_scene == nullptr || m_parameters.getFormat() == ImageFormat::Unknown || !m_parameters.isValid()) {
        RAISE_RUNTIME_ERROR << "Converter: TiffStructure is not initialized";
    }
}

std::string TiffConverter::SVSDateString() {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "Date = %m/%d/%Y", std::localtime(&time));
    std::string strDate(buffer);
    return strDate;
}

std::string TiffConverter::SVSTimeString() {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "Time = %H/%M/%S", std::localtime(&time));
    std::string strTime(buffer);
    return strTime;
}

void TiffConverter::checkSVSRequirements() const {
    const DataType dt = m_scene->getChannelDataType(0);
    const int numChannels = m_scene->getNumChannels();
    for (int channel = 1; channel < numChannels; ++channel) {
        if (dt != m_scene->getChannelDataType(channel)) {
            RAISE_RUNTIME_ERROR << "Converter: Cannot convert scene with different channel types to SVS!";
        }
    }
}

void TiffConverter::checkJpegRequirements() const {
    if (m_parameters.getEncoding() == Compression::Jpeg) {
        const int numChannels = m_scene->getNumChannels();
        if (m_parameters.getFormat() == ImageFormat::SVS) {
            if (numChannels != 1 && numChannels != 3) {
                RAISE_RUNTIME_ERROR << "Converter: Jpeg compression can be used for 1 and 3 channel images only!";
            }
        }
        for (int channel = 0; channel < numChannels; ++channel) {
            if (m_scene->getChannelDataType(channel) != DataType::DT_Byte) {
                RAISE_RUNTIME_ERROR << "Converter: Jpeg compression can be used for 8bit images only!";
            }
        }
    }
}

void TiffConverter::checkEncodingRequirements() const {
    if (m_parameters.getEncoding() == Compression::Jpeg) {
        checkJpegRequirements();
    }
}

void TiffConverter::checkContainerRequirements() const {
    if (m_parameters.getFormat() == ImageFormat::SVS) {
        checkSVSRequirements();
    }
}

void TiffConverter::updateNotDefinedParameters() {
    makeSureValid();
    m_parameters.updateNotDefinedParameters(m_scene);
}
