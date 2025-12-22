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
#include <filesystem>
#include <tinyxml2.h>

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
    std::shared_ptr<const TIFFContainerParameters> tiffParams = std::static_pointer_cast<const TIFFContainerParameters>(m_parameters.getContainerParameters());
    std::stringstream buff;
    buff << "SlideIO Library 2.0\n";
    buff << rect.width << "x" << rect.height;
    buff << "(" << tiffParams->getTileWidth() << "x" << tiffParams->getTileHeight() << ") ";
    if (m_parameters.getEncoding() == Compression::Jpeg) {
        std::shared_ptr<const JpegEncodeParameters> jpegParams = std::static_pointer_cast<const JpegEncodeParameters>(m_parameters.getEncodeParameters());
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
        
    } else if (m_parameters.getFormat() == ImageFormat::OME_TIFF) {
        return createOMETiffDescription();
    } else {
		RAISE_RUNTIME_ERROR << "Converter: Unrecognized target image format: " << static_cast<int>(m_parameters.getFormat());
    }
}

std::string TiffConverter::createOMETiffDescription() const{
	makeSureValid();
    tinyxml2::XMLDocument doc;
    auto* ome = doc.NewElement("OME");
    ome->SetAttribute("xmlns", "http://www.openmicroscopy.org/Schemas/OME/2016-06");
    ome->SetAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
    ome->SetAttribute("xsi:schemaLocation", "http://www.openmicroscopy.org/Schemas/OME/2016-06 http://www.openmicroscopy.org/Schemas/OME/2016-06/ome.xsd");
    doc.InsertFirstChild(ome);

    auto rect = m_scene->getRect();
    const int sizeX = rect.width;
    const int sizeY = rect.height;
    const int numChannels = m_parameters.getChannelRange().size();
    const int numZSlices = m_parameters.getSliceRange().size();
    const int numTFrames = m_parameters.getTFrameRange().size();
    double magnification = m_scene->getMagnification();
    if (magnification > 0) {
        auto* instrument = doc.NewElement("Instrument");
        auto* objective = doc.NewElement("Objective");
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
    pixels->SetAttribute("ID", "Pixels:0");
    pixels->SetAttribute("DimensionOrder", "XYCZT");
    pixels->SetAttribute("SizeX", sizeX);
    pixels->SetAttribute("SizeY", sizeY);
    pixels->SetAttribute("SizeZ", std::max(1, numZSlices));
    pixels->SetAttribute("SizeC", std::max(1, numChannels));
    pixels->SetAttribute("SizeT", std::max(1, numTFrames));

    // DataType mapping
    auto dt = m_scene->getChannelDataType(0);
    const char* typeStr = "uint8";
    switch (dt) {
    case DataType::DT_Byte: typeStr = "uint8"; break;
    case DataType::DT_UInt16: typeStr = "uint16"; break;
    case DataType::DT_Int16: typeStr = "int16"; break;
    case DataType::DT_Int32: typeStr = "int32"; break;
    case DataType::DT_Float32: typeStr = "float"; break;
    case DataType::DT_Float64: typeStr = "double"; break;
    default: typeStr = "uint8"; break;
    }
    pixels->SetAttribute("Type", typeStr);

    // Physical sizes
    Resolution res = m_scene->getResolution();
    if (res.x > 0) {
        pixels->SetAttribute("PhysicalSizeX", res.x * 1e6);
        pixels->SetAttribute("PhysicalSizeXUnit", "um");
    }
    if (res.y > 0) {
        pixels->SetAttribute("PhysicalSizeY", res.y * 1e6);
        pixels->SetAttribute("PhysicalSizeYUnit", "um");
    }

    image->InsertEndChild(pixels);

    // Channels
    for (const auto& channel : m_channels) {
        auto* xmlChannel = doc.NewElement("Channel");
        xmlChannel->SetAttribute("ID", channel.getID().c_str());
        xmlChannel->SetAttribute("SamplesPerPixel", 1);
        const std::string& channelName = channel.getName();
        if (!channelName.empty()) 
            xmlChannel->SetAttribute("Name", channel.getName().c_str());
        pixels->InsertEndChild(xmlChannel);
    }

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

    for (const auto& page: m_pages) {
        auto* tiffData = doc.NewElement("TiffData");
		cv::Range sliceRange = page.getZSliceRange();
		cv::Range channelRange = page.getChannelRange();
		cv::Range frameRange = page.getTFrameRange();
		tiffData->SetAttribute("IFD", ifd++);
        tiffData->SetAttribute("FirstC", channel);
        tiffData->SetAttribute("SizeC", channelRange.size());
        tiffData->SetAttribute("FirstZ", slice);
		tiffData->SetAttribute("SizeZ", sliceRange.size());
        tiffData->SetAttribute("FirstT", frame);
		tiffData->SetAttribute("SizeT", frameRange.size());
		tiffData->SetAttribute("PlaneCount", page.getPlaneCount());
        auto* uidElem = doc.NewElement("UUID");
        uidElem->SetAttribute("FileName", fileName.c_str());
        std::string UUID = Tools::randomUUID();
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
    m_channels.clear();
    m_file.reset();
    m_filePath.clear();
    m_totalTiles = 0;
    m_currentTile = 0;
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
                page.setChannelRange(cv::Range(channel, channel+channelChunkSize));
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

    if (hasChannelNames) {
 		int id = 0;
         for (int channel = channelRange.start; channel < channelRange.end; ++channel) {
             std::string idAttr = std::string("Channel:0:") + std::to_string(id++);
             std::string name = m_scene->getChannelName(channel);
             TiffChannel tiffChannel(name, idAttr, 1);
             m_channels.emplace_back(tiffChannel);
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
    if ( block.x + block.width > sceneRect.width) {
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

    std::shared_ptr<const TIFFContainerParameters> tiffParams = std::static_pointer_cast<const TIFFContainerParameters>(m_parameters.getContainerParameters());
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
        std::shared_ptr<const JpegEncodeParameters> jpegParams = std::static_pointer_cast<const JpegEncodeParameters>(m_parameters.getEncodeParameters());
        dir.compressionQuality = jpegParams->getQuality();
    }
    dir.description = page.getDescription();
    dir.res = m_scene->getResolution();
    return dir;
}

void TiffConverter::writeDirectoryData(TiffDirectory& dir, const TiffDirectoryStructure& page, const std::function<void(int)>& cb) {
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
    cv::Mat tile;
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
    for (int y = m_cropRect.y; y < yEnd; y += sceneTileSize.height) {
        for (int x = m_cropRect.x; x < xEnd; x += sceneTileSize.width) {
            cv::Rect blockRect(x, y, sceneTileSize.width, sceneTileSize.height);
            ConverterTools::readTile(m_scene, channels, zoomLevel, blockRect, slice, frame, tile);
            if (tile.rows != tileSize.height || tile.cols != tileSize.width) {
                RAISE_RUNTIME_ERROR << "Converter: Unexpected tile size ("
                    << tile.cols << ","
                    << tile.rows << "). Expected tile size: ("
                    << tileSize.width << ","
                    << tileSize.height << ").";
            }
            blockRect.x -= m_cropRect.x;
            blockRect.y -= m_cropRect.y;
            cv::Rect zoomLevelRect = ConverterTools::scaleRect(blockRect, zoomLevel, true);
            m_file->writeTile(zoomLevelRect.x, zoomLevelRect.y, dir.slideioCompression, *encoding, tile, buffer.data(), (int)buffer.size());
            m_currentTile++;
            if (cb) {
				double proc = 100. * (double)m_currentTile / (double)m_totalTiles;
                cb(std::lround(proc));
            }
        }
    }
}

void TiffConverter::createTiff(const std::string& filePath, const std::function<void(int)>& cb) {
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
        writeDirectoryData(dir, page, cb);
        m_file->writeDirectory();
        const int numSubdirs = page.getNumSubDirectories();
        for (int subDirIndex = 0; subDirIndex < numSubdirs; ++subDirIndex) {
            const TiffDirectoryStructure& dirSpec = page.getSubDirectory(subDirIndex);
            TiffDirectory subDir = setUpDirectory(dirSpec);
            m_file->setTags(subDir);
            writeDirectoryData(subDir, dirSpec, cb);
            m_file->writeDirectory();
        }
    }
}


void TiffConverter::makeSureValid() const {
    if (m_scene == nullptr || m_parameters.getFormat() == ImageFormat::Unknown || !m_parameters.isValid()) {
         RAISE_RUNTIME_ERROR << "Converter: TiffStructure is not initialized";
     }
 }

std::string TiffConverter::SVSDateString()
{
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "Date = %m/%d/%Y", std::localtime(&time));
    std::string strDate(buffer);
    return strDate;
}

std::string TiffConverter::SVSTimeString()
{
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "Time = %H/%M/%S", std::localtime(&time));
    std::string strTime(buffer);
    return strTime;
}

void TiffConverter::checkSVSRequirements() const
{
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
    if (m_parameters.getContainerType() == ImageFormat::SVS) {
        checkSVSRequirements();
    }
}

void TiffConverter::updateNotDefinedParameters() {
    makeSureValid();
    m_parameters.updateNotDefinedParameters(m_scene);
}
