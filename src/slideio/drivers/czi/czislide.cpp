// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/czi/czislide.hpp"
#include "slideio/drivers/czi/cziscene.hpp"
#include "slideio/drivers/czi/czistructs.hpp"
#include "slideio/core/tools/xmltools.hpp"
#include <filesystem>
#include <tinyxml2.h>
#include <set>

#include "czithumbnail.hpp"
#include "czitools.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/base/log.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/core/tools/endian.hpp"

using namespace slideio;
using namespace tinyxml2;

static char SID_FILES[] = "ZISRAWFILE";
static char SID_METADATA[] = "ZISRAWMETADATA";
static char SID_DIRECTORY[] = "ZISRAWDIRECTORY";
static char SID_ATTACHMENT_DIR[] = "ZISRAWATTDIR";
static char SID_ATTACHMENT_CONTENT[] = "ZISRAWATTACH";

using namespace slideio;

CZISlide::CZISlide(const std::string& filePath) : m_filePath(filePath), m_resZ(0), m_resT(0), m_magnification(0)
{
    init();
}

CZISlide::~CZISlide()
{
}

int CZISlide::getNumScenes() const
{
	return static_cast<int>(m_scenes.size());
}

std::string CZISlide::getFilePath() const
{
	return m_filePath;
}

std::shared_ptr<CVScene> CZISlide::getScene(int index) const
{
    if(index<0 || index>=getNumScenes()) {
        RAISE_RUNTIME_ERROR << "CZIImageDriver: Invalid scene index: " << index;
    }

	return m_scenes[index];
}

void CZISlide::readBlock(uint64_t pos, uint64_t size, std::vector<unsigned char>& data)
{
    try
    {
        data.resize(size);
        m_fileStream.seekg(pos, std::ios_base::beg);
        m_fileStream.read((char*)data.data(), size);
    }
    catch(std::exception& ex) {
        m_fileStream.clear();
        m_fileStream.seekg(0);
        throw ex;
    }
}

std::shared_ptr<CVScene> CZISlide::getAuxImage(const std::string& sceneName) const {
    auto it = m_auxImages.find(sceneName);
    if(it==m_auxImages.end()) {
        RAISE_RUNTIME_ERROR << "CZIImageDriver: unknown auxiliary image: " << sceneName;
    }
    return it->second;
}

void CZISlide::readAttachments()
{
    SLIDEIO_LOG(INFO) << "Reading slide attachments (position: " << m_attachmentDirectoryPosition << ")";
    try
    {
        if (m_attachmentDirectoryPosition > 0)
        {
            AttachmentDirectorySegment attachmentDirectory{};
            m_fileStream.seekg(m_attachmentDirectoryPosition, std::ios_base::beg);
            m_fileStream.read((char*)&attachmentDirectory, sizeof(attachmentDirectory));
			updateAttachmentDirectorySegmentBE(attachmentDirectory);
            if (strcmp(attachmentDirectory.header.SID, SID_ATTACHMENT_DIR) == 0) {
                SLIDEIO_LOG(INFO) << "Reading attachment header. Number of attachments: "
                    << m_attachmentDirectoryPosition
                    << attachmentDirectory.data.entryCount
                    << ")";
                const int32_t numbAttachments = attachmentDirectory.data.entryCount;
                std::ifstream::pos_type pos = m_fileStream.tellg();
                for (int attachment = 0; attachment < numbAttachments; ++attachment)
                {
                    SLIDEIO_LOG(INFO) << "Reading attachment " << attachment << ". Position: " << pos << ".";
                    m_fileStream.seekg(pos, std::ios_base::beg);
                    AttachmentEntry entry{ 0 };
                    m_fileStream.read(reinterpret_cast<char*>(&entry), sizeof(entry));
					updateAttachmentEntryBE(entry);
                    SLIDEIO_LOG(INFO) << "Attachment Schema Type:" << entry.schemaType;
                    SLIDEIO_LOG(INFO) << "Attachment Content Type:" << entry.contentFileType;
                    if (!m_fileStream) {
                        break;
                    }
                    if (strcmp(entry.schemaType, "A1") != 0) {
                        break;
                    }
                    if (strcmp(entry.contentFileType, "JPG") == 0 ||
                        strcmp(entry.contentFileType, "CZI") == 0)
                    {
                        try {
                            addAuxiliaryImage(entry.name, entry.contentFileType, entry.filePosition);
                        }
                        catch(std::exception& err) {
                            SLIDEIO_LOG(WARNING) << "Error reading auxiliary image: " << err.what();
                            m_fileStream.clear();
                        }
                    }
                    pos += 128;
                }
            }
        }
    }
    catch(std::exception& err) {
        SLIDEIO_LOG(WARNING) << "Error reading attachments: " << err.what();
        m_fileStream.clear();
    }
}

void CZISlide::init()
{
    SLIDEIO_LOG(INFO) << "Slide initialization. File path: " << getFilePath();
    // read file header
    m_fileStream.exceptions(std::ios::failbit | std::ios::badbit);
    auto flags = std::ifstream::in | std::ifstream::binary;
#if defined(WIN32)
    std::wstring wsPath = Tools::toWstring(getFilePath());
    m_fileStream.open(wsPath.c_str(), flags);
#else
    m_fileStream.open(m_filePath.c_str(), flags);
#endif
    readFileHeader();
    readMetadata();
    readDirectory();
    readAttachments();
    SLIDEIO_LOG(INFO) << "Auxiliary images (" << m_auxImages.size() << "):" << m_auxNames;
    SLIDEIO_LOG(INFO) << "Slide initialized successfully.";
}

void CZISlide::parseMagnification(XMLNode* root)
{
    const std::vector<std::string> magnificationPath = {
        "ImageDocument","Metadata","Information", "Instrument",
        "Objectives", "Objective", "NominalMagnification"
    };
    const XMLElement* xmlMagnification = XMLTools::getElementByPath(root, magnificationPath);
    if(xmlMagnification)
        m_magnification = xmlMagnification->FloatText(20.);
}

void CZISlide::parseMetadataXmL(const char* xmlString, size_t dataSize)
{
    XMLDocument doc;
    XMLError error = doc.Parse(xmlString, dataSize);
    if (error != XML_SUCCESS)  {
        RAISE_RUNTIME_ERROR << "CZIImageDriver: Error parsing metadata xml";
    }
    const std::vector<std::string> titlePath = {
        "ImageDocument","Metadata","Information", "Document","Title"
    };
    //doc.SaveFile(R"(D:\Temp\czi1.xml)");
    const XMLElement* xmlTitle = XMLTools::getElementByPath(&doc, titlePath);
    if(xmlTitle){
        m_title = xmlTitle->GetText();
    }
    parseSizes(&doc);
    parseMagnification(&doc);
    parseResolutions(&doc);
    parseChannels(&doc);
}

void CZISlide::parseChannels(XMLNode* root)
{
    const std::vector<std::string> imagePath = {
        "ImageDocument","Metadata","Information", "Image",
        "Dimensions", "Channels"
    };
    std::map<std::string, int> channelIds;
    const XMLElement* xmlChannels = XMLTools::getElementByPath(root, imagePath);
    if (xmlChannels != nullptr)
    {
        for (auto xmlChannel = xmlChannels->FirstChildElement("Channel");
            xmlChannel != nullptr; xmlChannel = xmlChannel->NextSiblingElement())
        {
            const char* name = xmlChannel->Name();
            if (name && strcmp(name, "Channel") == 0)
            {
                m_channels.emplace_back();
                CZIChannelInfo& channel = m_channels.back();
                const char* channelId = xmlChannel->Attribute("Id");
                if (channelId)
                {
                    channel.id = channelId;
                    channelIds[channelId] = static_cast<int>(m_channels.size()) - 1;
                }
                const char* channelName = xmlChannel->Attribute("Name");
                if (channelName)
                {
                    channel.name = channelName;
                }
            }
        }
    }
    else
    {
        // channel information is missing
        // see https://gitlab.com/bioslide/slideio/-/issues/16
        // create channel info based on "PixelType" xml entry
        const std::vector<std::string> pixelTypePath = {
            "ImageDocument","Metadata","Information", "Image",
            "PixelType"
        };
        const XMLElement* xmlPixelType = XMLTools::getElementByPath(root, pixelTypePath);
        if (!xmlPixelType) {
            RAISE_RUNTIME_ERROR << "CZIImageDriver: Invalid xml : no channel information";
        }
        int channelCount = CZITools::channelCountFromPixelType(xmlPixelType);
        m_channels.resize(channelCount);
    }
    const std::vector<std::string> displayInfoPath = {
        "ImageDocument","Metadata",
        "DisplaySetting", "Channels"
    };
    int currentIndex(0);
    const XMLElement* xmlDisplayChannels = XMLTools::getElementByPath(root, displayInfoPath);
    for (auto xmlDisplayChannel = xmlDisplayChannels->FirstChildElement("Channel");
        xmlDisplayChannel != nullptr; xmlDisplayChannel = xmlDisplayChannel->NextSiblingElement())
    {
        const char* name = xmlDisplayChannel->Name();
        if (name && strcmp(name, "Channel") == 0)
        {
            auto xmlShortName= xmlDisplayChannel->FirstChildElement("ShortName");
            if(xmlShortName)
            {
                const char* channelName = xmlShortName->GetText();
                const char* channelId = xmlDisplayChannel->Attribute("Id");
                if(channelName && channelId)
                {
                    auto idIt = channelIds.find(std::string(channelId));
                    if(idIt!=channelIds.end())
                    {
                        const int channelIndex = idIt->second;
                        m_channels[channelIndex].name = channelName;
                    }
                    else
                    {
                        // id is not in the id map. Most likely
                        // matadtata is not created according to
                        // the specs.
                        m_channels[currentIndex].name = channelName;
                    }
                }
            }
            currentIndex++;
        }
    }
}


void CZISlide::readMetadata()
{
    // position stream pointer to metadata segment
    m_fileStream.seekg(m_metadataPosition, std::ios_base::beg);
    // read segment header
    SegmentHeader header{};
    m_fileStream.read((char*)&header, sizeof(header));
	updateSegmentHeaderBE(header);
    if (strncmp(header.SID, SID_METADATA, sizeof(SID_METADATA)) != 0)
    {
        RAISE_RUNTIME_ERROR << "CZIImageDriver: invalid metadata segment in file: " << m_filePath;
    }
    // read metadata header
    MetadataHeader metadataHeader{};
    m_fileStream.read((char*)&metadataHeader, sizeof(metadataHeader));
	updateMetadataHeaderBE(metadataHeader);
    const uint32_t xmlSize = metadataHeader.xmlSize;;
    std::vector<char> xmlString(xmlSize);
    // read metadata xml
    m_fileStream.read(xmlString.data(), xmlSize);
    m_rawMetadata.assign(xmlString.data(), xmlSize);
    Tools::replaceAll(m_rawMetadata, "\r\n", "\n");
    parseMetadataXmL(xmlString.data(), xmlSize);
}

void CZISlide::readFileHeader(FileHeader& fileHeader) {
    fileHeader = {};
    uint64_t pos = m_fileStream.tellg();
    SegmentHeader header{};
    m_fileStream.read(reinterpret_cast<char*>(&header), sizeof(header));
    updateSegmentHeaderBE(header);
    if (strncmp(header.SID, SID_FILES, sizeof(SID_FILES)) != 0) {
        RAISE_RUNTIME_ERROR << "CZIImageDriver:" << m_filePath << " is not a CZI file.";
    }
    m_fileStream.read(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));
	updateFileHeaderBE(fileHeader);
	m_fileStream.seekg(pos);
}

void CZISlide::readFileHeader()
{
    FileHeader fileHeader;
    readFileHeader(fileHeader);
    m_directoryPosition = fileHeader.directoryPosition;
    m_metadataPosition = fileHeader.metadataPosition;
    m_attachmentDirectoryPosition = fileHeader.attachmentDirectoryPosition;
}

void CZISlide::readSubBlocks(uint64_t directoryPosition, uint64_t originPos, std::vector<CZISubBlocks>& sceneBlocks, std::vector<uint64_t>& sceneIds) {
    m_fileStream.seekg(directoryPosition + originPos, std::ios_base::beg);
    // read segment header
    SegmentHeader header{};
    m_fileStream.read(reinterpret_cast<char*>(&header), sizeof(header));
    updateSegmentHeaderBE(header);
    if (strncmp(header.SID, SID_DIRECTORY, sizeof(SID_DIRECTORY)) != 0) {
        RAISE_RUNTIME_ERROR << "CZIImageDriver: invalid directory segment of file " << m_filePath;
    }
    DirectoryHeader directoryHeader{};
    m_fileStream.read(reinterpret_cast<char*>(&directoryHeader), sizeof(directoryHeader));
	updateDirectoryHeaderBE(directoryHeader);
    std::map<uint64_t, int> sceneMap;
    auto filePos = m_fileStream.tellg();
    for (unsigned int entry = 0; entry < directoryHeader.entryCount; ++entry)
    {
        try
        {
            CZISubBlock block;
            DirectoryEntryDV entryHeader{};
            m_fileStream.seekg(filePos);
            m_fileStream.read(reinterpret_cast<char*>(&entryHeader), sizeof(entryHeader));
			updateDirectoryEntryBE(entryHeader);
            std::vector<DimensionEntryDV> dimensions(entryHeader.dimensionCount);
            for (int dim = 0; dim < entryHeader.dimensionCount; ++dim)
            {
                DimensionEntryDV& dimEntry = dimensions[dim];
                m_fileStream.read(reinterpret_cast<char*>(&dimEntry), sizeof(dimEntry));
				updateDimensionEntryBE(dimEntry);
            }
            filePos = m_fileStream.tellg();
            m_fileStream.seekg(entryHeader.filePosition + originPos);
            SegmentHeader segmentHeader;
            m_fileStream.read((char*)&segmentHeader, sizeof(segmentHeader));
			updateSegmentHeaderBE(segmentHeader);
            SubBlockHeader subblockHeader;
            m_fileStream.read((char*)&subblockHeader, sizeof(subblockHeader));
			updateSublockHeaderBE(subblockHeader);
            subblockHeader.direEntry.filePosition += originPos;
            block.setupBlock(subblockHeader, dimensions);
            const std::vector<Dimension>& blockDimensions = block.dimensions();
            std::vector<uint64_t> blockSceneIds;
            CZIScene::sceneIdsFromDims(blockDimensions, blockSceneIds);

            for(const auto& sceneId : blockSceneIds)
            {
                auto sceneIt = sceneMap.find(sceneId);
                int sceneIndex = 0;
                if(sceneIt==sceneMap.end())
                {
                    sceneIndex = static_cast<int>(sceneBlocks.size());
                    sceneBlocks.emplace_back();
                    sceneMap[sceneId] = sceneIndex;
                    sceneIds.push_back(sceneId);
                }
                else
                {
                    sceneIndex = sceneIt->second;
                }
                sceneBlocks[sceneIndex].push_back(block);
            }
        }
        catch (const std::ios_base::failure&)
        {
            SLIDEIO_LOG(WARNING) << "Error by reading of subblocks of the file " << getFilePath() << "." << std::endl;
            break;
        }
    }
}

std::shared_ptr<CZIScene> CZISlide::constructScene(const uint64_t sceneId, const CZISubBlocks& blocks)
{
    CZIScene::SceneParams params{};
    std::shared_ptr<CZIScene>scene(new CZIScene);
    CZIScene::dimsFromSceneId(sceneId, params);
    scene->init(sceneId, params, m_filePath, blocks, this);
    return scene;
}

void CZISlide::readDirectory()
{
    // position stream pointer to the directory segment
    std::vector<CZISubBlocks> sceneBlocks;
    std::vector<uint64_t> sceneIds;
    readSubBlocks(m_directoryPosition, 0, sceneBlocks, sceneIds);
    for(size_t sceneIndex = 0; sceneIndex < sceneBlocks.size(); ++sceneIndex)
    {
        const uint64_t sceneId = sceneIds[sceneIndex];
        const CZISubBlocks& blocks = sceneBlocks[sceneIndex];
        std::shared_ptr<CZIScene> scene = constructScene(sceneId, blocks);
        m_scenes.push_back(scene);
    }

}

void CZISlide::parseResolutions(XMLNode* root)
{
    const std::vector<std::string> scalingItemsPath = {
        "ImageDocument","Metadata","Scaling", "Items"
    };
    // resolutions
    bool timeResolutionSet = false;
    const XMLElement* xmlItems = XMLTools::getElementByPath(root, scalingItemsPath);
    for (auto child = xmlItems->FirstChildElement(); child != nullptr;
        child = child->NextSiblingElement())
    {
        const char* name = child->Name();
        if (name && strcmp(name, "Distance") == 0)
        {
            const char* id = child->Attribute("Id");
            if (id)
            {
                const XMLElement* valueElement = child->FirstChildElement("Value");
                if (valueElement)
                {
                    const double value = valueElement->DoubleText(0);
                    if (strcmp("X", id) == 0) {
                        m_res.x = value;
                    }
                    else if (strcmp("Y", id) == 0) {
                        m_res.y = value;
                    }
                    else if (strcmp("Z", id) == 0) {
                        m_resZ = value;
                    }
                    else if (strcmp("T", id) == 0) {
                        m_resT = value;
                        timeResolutionSet = m_resT > 0.;
                    }
                }
            }
        }
    }
    if (!timeResolutionSet) {
        // fix for the issue: https://gitlab.com/bioslide/slideio/-/issues/14
        const std::vector<std::string> scalingItemsPath2 = {
            "ImageDocument","Metadata","Information", "Image","Dimensions","T","Positions","Interval","Increment"
        };
        const XMLElement* xmlItems2 = XMLTools::getElementByPath(root, scalingItemsPath2);
        if (xmlItems2) {
            m_resT = xmlItems2->DoubleText(0);
        }
    }
}

void CZISlide::parseSizes(tinyxml2::XMLNode* root)
{
    const std::vector<std::string> imagePath = { "ImageDocument","Metadata","Information", "Image" };
    const XMLElement* xmlImage = XMLTools::getElementByPath(root, imagePath);
    m_slideXs = XMLTools::childNodeTextToInt(xmlImage, "SizeX");
    m_slideYs = XMLTools::childNodeTextToInt(xmlImage, "SizeY");
    m_slideZs = XMLTools::childNodeTextToInt(xmlImage, "SizeZ");
    m_slideTs = XMLTools::childNodeTextToInt(xmlImage, "SizeT");
    m_slideRs = XMLTools::childNodeTextToInt(xmlImage, "SizeR");
    m_slideIs = XMLTools::childNodeTextToInt(xmlImage, "SizeI");
    m_slideSs = XMLTools::childNodeTextToInt(xmlImage, "SizeS");
    m_slideHs = XMLTools::childNodeTextToInt(xmlImage, "SizeH");
    m_slideMs = XMLTools::childNodeTextToInt(xmlImage, "SizeM");
    m_slideBs = XMLTools::childNodeTextToInt(xmlImage, "SizeB");
    m_slideVs = XMLTools::childNodeTextToInt(xmlImage, "SizeV");
}


void CZISlide::addAuxiliaryImage(const std::string& name, const std::string& typeName, int64_t position)
{
    SLIDEIO_LOG(INFO) << "Reading Auxiliary Image:" << name << ".Type: " << typeName << ".Position: " << position;
    m_fileStream.seekg(position, std::ios_base::beg);
    AttachmentSegment attachmentSegment;
    m_fileStream.read(reinterpret_cast<char*>(&attachmentSegment), sizeof(attachmentSegment));
	updateAttachmentSegmentBE(attachmentSegment);
    if (strcmp(attachmentSegment.header.SID, SID_ATTACHMENT_CONTENT) == 0)
    {
        int64_t dataSize = attachmentSegment.data.dataSize;
        const int64_t dataPosition = position + 256;
        if (typeName.compare("CZI") == 0) {
            createCZIAttachmentScenes(dataPosition, dataSize, name);
        }
        else if (typeName.compare("JPG") == 0) {
            createJpgAttachmentScenes(dataPosition, dataSize, name);
        }
        else {
            RAISE_RUNTIME_ERROR << "CZIImageDriver: unexpected attachment image type " << typeName;
        }
    }
}

void CZISlide::createCZIAttachmentScenes(const int64_t dataPos, int64_t dataSize, const std::string& attachmentName)
{
    const int64_t fileOrigin = dataPos + sizeof(SegmentHeader);
    m_fileStream.seekg(fileOrigin);
    FileHeader fileHeader{};
    readFileHeader(fileHeader);
    std::vector<CZISubBlocks> sceneBlocks;
    std::vector<uint64_t> sceneIds;
    readSubBlocks(fileHeader.directoryPosition, fileOrigin, sceneBlocks, sceneIds);
    const bool multiScene = sceneIds.size() > 1;
    for (size_t sceneIndex = 0; sceneIndex < sceneBlocks.size(); ++sceneIndex)
    {
        const uint64_t sceneId = sceneIds[sceneIndex];
        const CZISubBlocks& blocks = sceneBlocks[sceneIndex];
        std::shared_ptr<CZIScene> scene = constructScene(sceneId, blocks);
        std::string sceneName = attachmentName;
        if (multiScene) {
            sceneName += std::string("(") + std::to_string(sceneIndex + 1) + std::string(")");
        }
        m_auxImages[sceneName] = scene;
        m_auxNames.push_back(sceneName);
    }
}

void CZISlide::createJpgAttachmentScenes(const int64_t dataPosition, int64_t dataSize, const std::string& name)
{
    const int64_t fileOrigin = dataPosition + sizeof(SegmentHeader);
    std::shared_ptr<CZIThumbnail> thumbnail(new CZIThumbnail);
    thumbnail->setAttachmentData(this, fileOrigin, dataSize, name);
    if (thumbnail->init()) {
        std::shared_ptr<CVScene> attachment = thumbnail;
        m_auxImages[name] = attachment;
        m_auxNames.push_back(name);
    }
}

void CZISlide::updateSegmentHeaderBE(SegmentHeader& header)
{
	if (Endian::isLittleEndian())
		return;
	header.allocatedSize = Endian::fromLittleEndianToNative(header.allocatedSize);
	header.usedSize = Endian::fromLittleEndianToNative(header.usedSize);
}
void CZISlide::updateFileHeaderBE(FileHeader& header)
{
    if (Endian::isLittleEndian())
        return;
    header.majorVersion = Endian::fromLittleEndianToNative(header.majorVersion);
	header.minorVerion = Endian::fromLittleEndianToNative(header.minorVerion);
	header.directoryPosition = Endian::fromLittleEndianToNative(header.directoryPosition);
	header.metadataPosition = Endian::fromLittleEndianToNative(header.metadataPosition);
	header.updatePending = Endian::fromLittleEndianToNative(header.updatePending);
	header.attachmentDirectoryPosition = Endian::fromLittleEndianToNative(header.attachmentDirectoryPosition);
}

void CZISlide::updateMetadataHeaderBE(MetadataHeader& header)
{
    if (Endian::isLittleEndian())
        return;
    header.xmlSize = Endian::fromLittleEndianToNative(header.xmlSize);
	header.attachmentSize = Endian::fromLittleEndianToNative(header.attachmentSize);
}

void CZISlide::updateDirectoryHeaderBE(DirectoryHeader& header)
{
    if (Endian::isLittleEndian())
        return;
    header.entryCount = Endian::fromLittleEndianToNative(header.entryCount);
}

void CZISlide::updateDirectoryEntryBE(DirectoryEntryDV& entry)
{
    if (Endian::isLittleEndian())
        return;
    entry.pixelType = Endian::fromLittleEndianToNative(entry.pixelType);
	entry.filePosition = Endian::fromLittleEndianToNative(entry.filePosition);
	entry.filePart = Endian::fromLittleEndianToNative(entry.filePart);
	entry.compression = Endian::fromLittleEndianToNative(entry.compression);
	entry.dimensionCount = Endian::fromLittleEndianToNative(entry.dimensionCount);
}

void CZISlide::updateDimensionEntryBE(DimensionEntryDV& entry) {
    if (Endian::isLittleEndian())
        return;
    entry.start = Endian::fromLittleEndianToNative(entry.start);
	entry.size = Endian::fromLittleEndianToNative(entry.size);
	entry.storedSize = Endian::fromLittleEndianToNative(entry.storedSize);
	entry.startCoordinate = Endian::fromLittleEndianToNative(entry.startCoordinate);
}

void CZISlide::updateSublockHeaderBE(SubBlockHeader& header) {
    if (Endian::isLittleEndian())
        return;
    header.metadataSize = Endian::fromLittleEndianToNative(header.metadataSize);
	header.attachmentSize = Endian::fromLittleEndianToNative(header.attachmentSize);
	header.dataSize = Endian::fromLittleEndianToNative(header.dataSize);
	updateDirectoryEntryBE(header.direEntry);
    
}

void CZISlide::updateAttachmentEntryBE(AttachmentEntry& entry) {
    if (Endian::isLittleEndian())
        return;
    entry.filePosition = Endian::fromLittleEndianToNative(entry.filePosition);
	entry.filePart = Endian::fromLittleEndianToNative(entry.filePart);
}

void CZISlide::updateAttachmentDirectorySegmentDataBE(AttachmentDirectorySegmentData& data) {
	data.entryCount = Endian::fromLittleEndianToNative(data.entryCount);
}

void CZISlide::updateAttachmentDirectorySegmentBE(AttachmentDirectorySegment& segment) {
    if (Endian::isLittleEndian())
        return;
    updateSegmentHeaderBE(segment.header);
	updateAttachmentDirectorySegmentDataBE(segment.data);
}

void CZISlide::updateAttachmentEntryA1BE(AttachmentEntryA1& entry) {
    if (Endian::isLittleEndian())
        return;
    entry.filePosition = Endian::fromLittleEndianToNative(entry.filePosition);
	entry.filePart = Endian::fromLittleEndianToNative(entry.filePart);
}

void CZISlide::updateAttachmentSegmentDataBE(AttachmentSegmentData& data) {
    if (Endian::isLittleEndian())
        return;
    data.dataSize = Endian::fromLittleEndianToNative(data.dataSize);
    updateAttachmentEntryA1BE(data.attachmentEntry);
}

void CZISlide::updateAttachmentSegmentBE(AttachmentSegment& segment)
{
    if (Endian::isLittleEndian())
        return;
    updateSegmentHeaderBE(segment.header);
	updateAttachmentSegmentDataBE(segment.data);
}

void CZISlide::updateDimensionBE(Dimension& dim) {
    if (Endian::isLittleEndian())
        return;
    dim.start = Endian::fromLittleEndianToNative(dim.start);
	dim.size = Endian::fromLittleEndianToNative(dim.size);
}
