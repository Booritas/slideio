
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "vsifile.hpp"
#include <iomanip>
#include "slideio/core/tools/tools.hpp"
#include <boost/filesystem.hpp>
#include "vsistruct.hpp"
#include "vsitags.hpp"
#include "vsistream.hpp"
#include "etsfile.hpp"
#include "vsitools.hpp"
#include <boost/tokenizer.hpp>
#include <boost/foreach.hpp>

#include "slideio/imagetools/tifftools.hpp"

using namespace slideio;
namespace fs = boost::filesystem;


vsi::VSIFile::VSIFile(const std::string& filePath) : m_filePath(filePath)
{
    read();
}

std::string vsi::VSIFile::getRawMetadata() const
{
    return boost::json::serialize(m_metadata);
}

void vsi::VSIFile::read()
{
    SLIDEIO_LOG(INFO) << "VSI driver: reading file " << m_filePath;
    readVolumeInfo();
    TiffTools::scanFile(m_filePath, m_directories);
    if (m_hasExternalFiles) {
        readExternalFiles();
    }
    for(auto& directory : m_directories) {

    }
}

boost::json::object findObject(boost::json::object& parent, const std::string& path)
{
    typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

    boost::char_separator<char> sep("/");
    tokenizer tokens(path, sep);

    boost::json::object current = parent;

    for(std::string const& token : tokens)
    {
        auto it = current.find(token);
        if(it == current.end()) {
            return {};
        }
        current = it->value().as_object();
    }
    return current;
}

boost::json::value findObject(boost::json::object& parent, const std::vector<int>& path)
{
    boost::json::value current = parent;
    boost::json::value empty(nullptr);

    for (const int tag: path)
    {
        if (!current.is_object()) {
            return empty;
        }
        auto currentParent = current.as_object();
        std::string token = std::to_string(tag);
        auto it = currentParent.find(token);
        if (it == currentParent.end()) {
            return empty;
        }
        current = it->value();
    }
    return current;
}

void vsi::VSIFile::checkExternalFilePresence()
{
    const std::vector<int> pathToImages = { Tag::COLLECTION_VOLUME, Tag::MULTIDIM_IMAGE_VOLUME };
    const std::vector<int> pathToExternalFiles = { Tag::IMAGE_FRAME_VOLUME, Tag::EXTERNAL_FILE_PROPERTIES, Tag::HAS_EXTERNAL_FILE };
    auto value = findObject(m_metadata, pathToImages);
    SLIDEIO_LOG(INFO) << "VSI driver: checking external file presence";
    if(value.is_array()) {
        auto volumes = value.as_array();
        for (auto& volume: volumes) {
            boost::json::object imageObject = volume.as_object();
            boost::json::value externalFile = findObject(imageObject, pathToExternalFiles);// "2002/2018/20005");
            if(externalFile.is_object()) {
                auto externalFileObject = externalFile.as_object();
                const auto itValue = externalFileObject.find("value");
                if (itValue == externalFileObject.end()) {
                     continue;
                }
                m_hasExternalFiles = itValue->value().as_string() == std::string("1");
            }
            SLIDEIO_LOG(INFO) << "VSI driver: external files are " << (m_hasExternalFiles ? "present" : "absent");
            if (m_hasExternalFiles)
                break;
        }
    }
}

void vsi::VSIFile::readVolumeInfo()
{
    SLIDEIO_LOG(INFO) << "VSI driver: reading volume info";
#if defined(WIN32)
    const std::wstring filePathW = Tools::toWstring(m_filePath);
    std::ifstream ifs(filePathW, std::ios::binary);
#else
    std::ifstream ifs(m_filePath, std::ios::binary);
#endif
    vsi::VSIStream vsiStream(ifs);
    vsi::ImageFileHeader header;
    vsiStream.read<vsi::ImageFileHeader>(header);
    if (strncmp(reinterpret_cast<char*>(header.magic), "II", 2) != 0) {
        RAISE_RUNTIME_ERROR << "VSI driver: invalid file header. Expected first two bytes: 'II', got: "
            << header.magic;
    }
    if (header.i42 != 42) {
        RAISE_RUNTIME_ERROR << "VSI driver: invalid file header. Expected second word: 42, got: "
            << header.i42;
    }
    boost::json::object root;
    readMetadata(vsiStream, root);
    m_metadata = root;
    bool empty = m_metadata.empty();
    {
        std::ofstream ofs("d:\\Temp\\metadata.json");
        ofs << boost::json::serialize(m_metadata);
        ofs.close();
    }
    checkExternalFilePresence();
}


void vsi::VSIFile::readExternalFiles()
{
    SLIDEIO_LOG(INFO) << "VSI driver: reading external ETS files";
    const fs::path filePath(m_filePath);
    const fs::path dirPath = filePath.parent_path();
    const fs::path fileName = filePath.filename();
    const fs::path subDirName = "_" + fileName.stem().string() + "_";
    const fs::path subDirPath = dirPath / subDirName;
    if (fs::exists(subDirPath)) {
        const std::list<std::string> files = Tools::findFilesWithExtension(subDirPath.string(), ".ets");
        for (const auto& file : files) {
            auto etsFile = std::make_shared<vsi::EtsFile>(file);
            etsFile->read();
            m_etsFiles.push_back(etsFile);
        }
    }
}


void vsi::VSIFile::readExtendedType(vsi::VSIStream& vsi, const vsi::TagInfo& tagInfo, boost::json::object& tagObject)
{
    switch(tagInfo.extendedType) {
    case ExtendedType::NEW_VOLUME_HEADER:
        {
            const int64_t endPointer = vsi.getPos() + tagInfo.dataSize;
            while (vsi.getPos() < endPointer && vsi.getPos() < vsi.getSize())
            {
                const int64_t start = vsi.getPos();
                bool ok = readMetadata(vsi, tagObject);
                if (!ok) {
                    break;
                }
                const int64_t end = vsi.getPos();
                if (start >= end) {
                    break;
                }
            }
        }
        break;
    case ExtendedType::PROPERTY_SET_VOLUME:
    case ExtendedType::NEW_MDIM_VOLUME_HEADER:
        readMetadata(vsi, tagObject);
        break;
    }
}

bool vsi::VSIFile::readMetadata(VSIStream& vsi, boost::json::object& parentObject)
{
    SLIDEIO_LOG(INFO) << "VSI driver: reading metadata";
    const int64_t headerPos = vsi.getPos();
    vsi::VolumeHeader volumeHeader = {};
    vsi.read<vsi::VolumeHeader>(volumeHeader);
    if (volumeHeader.headerSize != 24) {
        return false;
    }
    if (volumeHeader.magicNumber != 21321) {
        return false;
    }
    if (volumeHeader.offsetFirstDataField < 0) {
        return false;
    }
    const int64_t dataFieldOffset = headerPos + volumeHeader.offsetFirstDataField;

    if (dataFieldOffset >= vsi.getSize()) {
        return false;
    }
    const uint32_t volumeTagCount = volumeHeader.flags & vsi::VOLUME_TAG_COUNT_MASK;
    vsi.setPos(dataFieldOffset);

    struct vsi::TagHeader tagHeader;
    parentObject["tagCount"] = volumeTagCount;
    for (uint tagIndex = 0; tagIndex < volumeTagCount; ++tagIndex) {
        TagInfo tagInfo;
        int64_t tagPos = vsi.getPos();
        std::string storedValue;
        vsi.read(tagHeader);
        int32_t nextField = tagHeader.nextField & 0xFFFFFFFFL;
        const bool extraTag = ((tagHeader.fieldType & 0x8000000) >> 27) == 1;
        const bool extendedField = ((tagHeader.fieldType & 0x10000000) >> 28) == 1;
        const bool inlineData = ((tagHeader.fieldType & 0x40000000) >> 30) == 1;

        tagInfo.tag = tagHeader.tag;
        tagInfo.fieldType = tagHeader.fieldType;
        if(extendedField) {
            tagInfo.extendedType = static_cast<ExtendedType>(tagHeader.fieldType & 0xffffff);
        }
        else {
            tagInfo.valueType = static_cast<ValueType>(tagHeader.fieldType & 0xffffff);
        }

        if (extraTag) {
            vsi.read<int>(tagInfo.secondTag);
        }
        if (tagHeader.tag < 0) {
            if (!inlineData && (tagHeader.dataSize + vsi.getPos()) < vsi.getSize()) {
                vsi.skipBytes(tagHeader.dataSize);
            }
            return false;
        }

        tagInfo.dataSize = tagHeader.dataSize;

        std::string tagName = VSITools::getTagName(tagInfo, parentObject);
        std::string tagKey = std::to_string(static_cast<int>(tagInfo.tag));
        boost::json::object tagObject;
        tagObject["tag"] = static_cast<int>(tagInfo.tag);
        tagObject["name"] = tagName;
        tagObject["fieldType"] = tagHeader.fieldType;
        tagObject["valueType"] = static_cast<int>(tagInfo.valueType);
        tagObject["extendedType"] = static_cast<int>(tagInfo.extendedType);
        tagObject["extraTag"] = extraTag;
        tagObject["extendedField"] = extendedField;
        tagObject["secondTag"] = tagInfo.secondTag;

        if(extendedField) {
            readExtendedType(vsi, tagInfo, tagObject);
        }
        else {
            std::string value = inlineData ? std::to_string(tagInfo.dataSize) : " ";
            if (!inlineData && tagInfo.dataSize > 0) {
                value = VSITools::extractTagValue(vsi, tagInfo);
            }

            if (tagInfo.tag == vsi::Tag::DOCUMENT_TIME || tagInfo.tag == vsi::Tag::CREATION_TIME) {
                std::ostringstream oss;
                time_t time = std::stoll(value);
                oss << std::put_time(std::localtime(&time), "%d-%m-%Y %H-%M-%S");
                value = oss.str();
            }
            tagObject["value"] = value;
        }

        if (vsi::VSITools::isArray(tagInfo)) {
            auto it = parentObject.find(tagKey);
            if (it != parentObject.end()) {
                auto& array = it->value().as_array().emplace_back(std::move(tagObject));
            }
            else {
                boost::json::array array;
                array.emplace_back(std::move(tagObject));
                parentObject[tagKey] = std::move(array);
            }

        }
        else {
            if(parentObject.find(tagKey)!=parentObject.end()) {
                std::cout << "Duplicate tag " << tagKey << std::endl;
            }
            parentObject[tagKey] = std::move(tagObject);
        }


        if (nextField == 0) {
            if (headerPos + tagInfo.dataSize + 32 < vsi.getSize() && headerPos + tagInfo.dataSize >= 0) {
                vsi.setPos(headerPos + tagInfo.dataSize + 32);
            }
            return true;
        }

        if (headerPos + nextField < vsi.getSize() && headerPos + nextField >= 0) {
            vsi.setPos(headerPos + nextField);
        }
        else
            break;

    }
    return true;
}

