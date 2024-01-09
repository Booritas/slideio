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
#include <boost/algorithm/string.hpp>

#include "taginfo.hpp"
#include "slideio/imagetools/tifftools.hpp"

using namespace slideio;
using namespace slideio::vsi;

namespace fs = boost::filesystem;

static int extractBaseDirectoryNameSuffix(const fs::path& path) {
    std::string dirName;
    int count = 0;
    for (auto& part = path.rbegin(); part != path.rend(); part++) {
        if (count > 0) {
            dirName = part->string();
            break;
        }
        count++;
    }
    int suffix = std::stoi(dirName.c_str() + 5); // skip 'stack' prefix
    return suffix;
}


void vsi::VSIFile::extractVolumesFromMetadata() {

    const std::vector<int> REL_PATH_TO_MAGNIFICATION = {
        vsi::Tag::MICROSCOPE,
        vsi::Tag::MICROSCOPE_PROPERTIES,
        vsi::Tag::OPTICAL_PROPERTIES,
        vsi::Tag::OBJECTIVE_MAG
    };

    const std::vector<int> REL_PATH_TO_BITDEPTH = {
        vsi::Tag::MICROSCOPE,
        vsi::Tag::MICROSCOPE_PROPERTIES,
        vsi::Tag::OPTICAL_PROPERTIES,
        vsi::Tag::BIT_DEPTH
    };

    int countExternalVolumes = 0;
    int countInternalVolumes = 0;
    int countSkippedVolumes = 0;
    if(!m_metadata.empty()) {
        std::list<const TagInfo*> volumes;
        getVolumeMetadataItems(volumes);
        for(const TagInfo* const& volume :volumes) {
            std::list<const TagInfo*> frames;
            getImageFrameMetadataItems(volume, frames);
            for(auto& frame:frames) {
                std::shared_ptr<Volume> volumeObj = std::make_shared<vsi::Volume>();
                const TagInfo* externalFile = frame->findChild(vsi::Tag::EXTERNAL_FILE_PROPERTIES);
                const TagInfo* hasExternalFile = (externalFile==nullptr)?nullptr: externalFile->findChild(vsi::Tag::HAS_EXTERNAL_FILE);
                const TagInfo* ifd = frame->findChild(vsi::Tag::DEFAULT_SAMPLE_PIXEL_DATA_IFD);
                const TagInfo* stackProps = volume->findChild(vsi::Tag::MULTIDIM_STACK_PROPERTIES);
                const TagInfo* color = volume->findChild(Tag::DEFAULT_BACKGROUND_COLOR);
                if(color) {
                    try {
                        volumeObj->setDefaultColor(std::stoi(color->value));
                    }
                    catch (std::exception& ex) {
                        SLIDEIO_LOG(WARNING) << "VSI driver: error reading default color (ignored): " << ex.what();
                    }
                }
                if(stackProps) {
                    bool validVolume = false;
                    const TagInfo* stackName = stackProps->findChild(vsi::Tag::STACK_NAME);
                    if(stackName) {
                        volumeObj->setName(stackName->value);
                    }
                    const TagInfo* stackType = stackProps->findChild(vsi::Tag::STACK_TYPE);
                    if(stackType) {
                        try {
                            const int val = std::stoi(stackType->value);
                            volumeObj->setType(VSITools::intToStackType(val));
                        }
                        catch (std::exception& ex) {
                            SLIDEIO_LOG(WARNING) << "VSI driver: error reading stack type (ignored): " << ex.what();
                        }
                    }
                    if(externalFile) {
                        const TagInfo* boundary = externalFile->findChild(Tag::IMAGE_BOUNDARY);
                        if(boundary) {
                            std::string value = boundary->value;
                            value.erase(std::remove(value.begin(), value.end(), '('), value.end());
                            value.erase(std::remove(value.begin(), value.end(), ')'), value.end());
                            std::vector<std::string> tokens;
                            boost::split(tokens, value, boost::is_any_of(","));
                            if (tokens.size() == 4) {
                                const int width = std::stoi(tokens[2]);
                                const int height = std::stoi(tokens[3]);
                                volumeObj->setSize(cv::Size(width, height));
                            }
                        }
                    }
                    if (hasExternalFile) {
                        volumeObj->setHasExternalFile(true);
                        countExternalVolumes++;
                        validVolume = true;
                    }
                    else if (ifd) {
                        volumeObj->setIFD(ifd->secondTag);
                        countInternalVolumes++;
                        validVolume = true;
                    }
                    else {
                        countSkippedVolumes++;
                        continue;
                    }
                    const vsi::TagInfo* magnification = stackProps->findChild(REL_PATH_TO_MAGNIFICATION);
                    if(magnification) {
                        try {
                            volumeObj->setMagnification(std::stod(magnification->value));
                        }
                        catch (std::exception& ex) {
                            SLIDEIO_LOG(WARNING) << "VSI driver: error reading magnification (ignored): " << ex.what();
                        }
                    }
                    const TagInfo* bitDepth = stackProps->findChild(REL_PATH_TO_BITDEPTH);
                    if(bitDepth) {
                        try {
                            volumeObj->setBitDepth(std::stoi(bitDepth->value));
                        }
                        catch (std::exception& ex) {
                            SLIDEIO_LOG(WARNING) << "VSI driver: error reading bit depth (ignored): " << ex.what();
                        }
                    }
                    if (validVolume) {
                        m_volumes.push_back(volumeObj);
                    }
                }
            }
        }
    }
}

vsi::VSIFile::VSIFile(const std::string& filePath) : m_filePath(filePath) {
    m_metadata.tag = vsi::Tag::ROOT;
    m_metadata.name = "root";
    read();
}

std::string vsi::VSIFile::getRawMetadata() const {
    boost::json::object jsonMtd     ;
    serializeMetatdata(m_metadata, jsonMtd);
    return boost::json::serialize(jsonMtd);
}


void vsi::VSIFile::assignAuxImages() {
    try {
        std::sort(m_etsFiles.begin(), m_etsFiles.end(),
                  [](const std::shared_ptr<vsi::EtsFile>& left, const std::shared_ptr<vsi::EtsFile>& right) {
                      const int leftStackId = extractBaseDirectoryNameSuffix(left->getFilePath());
                      const int rightStackId = extractBaseDirectoryNameSuffix(right->getFilePath());
                      return leftStackId < rightStackId;
                  });
        std::shared_ptr<Volume> lastImageVolume;
        for (std::shared_ptr<Volume>& volume : m_volumes) {
            const auto volumeType = volume->getType();
            switch (volumeType) {
            case StackType::DEFAULT_IMAGE:
            case StackType::OVERVIEW_IMAGE:
                lastImageVolume = volume;
                break;
            case StackType::SAMPLE_MASK:
            case StackType::FOCUS_IMAGE:
            case StackType::EFI_SHARPNESS_MAP:
            case StackType::EFI_HEIGHT_MAP:
            case StackType::EFI_TEXTURE_MAP:
            case StackType::EFI_STACK:
            case StackType::MACRO_IMAGE:
                if (lastImageVolume) {
                    lastImageVolume->addAuxVolume(volume);
                }
                break;
            }
        }
    }
    catch (std::exception& ex) {
        SLIDEIO_LOG(ERROR) << "VSI driver: error reading external files: " << ex.what();
    }
}

void VSIFile::getVolumeMetadataItems(std::list<const TagInfo*>& volumes) const {
    const TagInfo* volumeCollection = m_metadata.findChild(vsi::Tag::COLLECTION_VOLUME);
    if (volumeCollection) {
        for (auto itPos = volumeCollection->begin(); itPos != volumeCollection->end(); ++itPos) {
            auto itVolume = volumeCollection->findNextChild(vsi::Tag::MULTIDIM_IMAGE_VOLUME, itPos);
            if(itVolume==volumeCollection->end())
                break;
            volumes.push_back(&*itVolume);
            itPos = itVolume;
        }
    }
}

void VSIFile::getImageFrameMetadataItems(const TagInfo* volume, std::list<const TagInfo*>& frames){
    for(auto itPos = volume->begin(); itPos != volume->end(); ++itPos) {
        auto itFrame = volume->findNextChild(vsi::Tag::IMAGE_FRAME_VOLUME, itPos);
        if (itFrame == volume->end())
            break;
        frames.push_back(&*itFrame);
        itPos = itFrame;
    }
}

void VSIFile::read() {
    SLIDEIO_LOG(INFO) << "VSI driver: reading file " << m_filePath;
    readVolumeInfo();
    checkExternalFilePresence();
    extractVolumesFromMetadata();
    TiffTools::scanFile(m_filePath, m_directories);
    if (m_expectExternalFiles) {
        readExternalFiles();
        assignAuxImages();
        // Assign volumes to external files
        std::vector<std::shared_ptr<Volume>> imageVolumes;
        for(auto& volume: m_volumes) {
            const auto stackType = volume->getType();
            if(stackType==StackType::DEFAULT_IMAGE || stackType==StackType::OVERVIEW_IMAGE) {
                imageVolumes.push_back(volume);
            }
        }
        if(imageVolumes.size() == m_etsFiles.size()) {
            // we assume that the order of volumes and ets files is the same
            for (size_t index = 0; index < imageVolumes.size(); index++) {
                m_etsFiles[index]->assignVolume(imageVolumes[index]);
            }
        }
    }
}

boost::json::object findObject(boost::json::object& parent, const std::string& path) {
    typedef boost::tokenizer<boost::char_separator<char>> tokenizer;

    boost::char_separator<char> sep("/");
    tokenizer tokens(path, sep);

    boost::json::object current = parent;

    for (std::string const& token : tokens) {
        auto it = current.find(token);
        if (it == current.end()) {
            return {};
        }
        current = it->value().as_object();
    }
    return current;
}

void vsi::VSIFile::checkExternalFilePresence() {
    SLIDEIO_LOG(INFO) << "VSI driver: checking external file presence";
    std::list<const TagInfo*> volumes;
    getVolumeMetadataItems(volumes);
    for(auto& volume: volumes) {
        std::list<const TagInfo*> frames;
        getImageFrameMetadataItems(volume, frames);
        for(auto& frame: frames) {
            const TagInfo* externalFile = frame->findChild(vsi::Tag::EXTERNAL_FILE_PROPERTIES);
            if(externalFile) {
                const TagInfo* hasExternalFile = externalFile->findChild(vsi::Tag::HAS_EXTERNAL_FILE);
                if (hasExternalFile) {
                    const std::string& value = hasExternalFile->value;
                    m_expectExternalFiles = value == std::string("1");
                    break;
                }
            }
        }
        if (m_expectExternalFiles)
            break;
    }
    SLIDEIO_LOG(INFO) << "VSI driver: external files are " << (m_expectExternalFiles ? "present" : "absent");
}

void vsi::VSIFile::serializeMetatdata(const TagInfo& tagInfo, boost::json::object& jsonObj) const {
    jsonObj["tag"] = tagInfo.tag;
    jsonObj["name"] = tagInfo.name;
    jsonObj["value"] = tagInfo.value;
    if(!tagInfo.children.empty()) {
        boost::json::array array;
        for(const auto& child: tagInfo.children) {
            boost::json::object childObject;
            serializeMetatdata(child, childObject);
            array.emplace_back(std::move(childObject));
        }
        jsonObj["value"] = std::move(array);
    }
    else {
        for (const auto& child : tagInfo.children) {
            boost::json::object childObject;
            serializeMetatdata(child, childObject);
            std::string tag = std::to_string(child.tag);
            jsonObj[tag] = std::move(childObject);
        }
    }
}

void vsi::VSIFile::readVolumeInfo() {
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
    //boost::json::object root;
    readMetadata(vsiStream, m_metadata);
    //serializeMetatdata(root, m_metadata);
    //m_metadata = root;
    //bool empty = m_metadata.empty();
    {
        //std::ofstream ofs("d:\\Temp\\metadata.json");
        //ofs << boost::json::serialize(m_metadata);
        //ofs.close();
    }
    checkExternalFilePresence();
}


void vsi::VSIFile::readExternalFiles() {
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


void vsi::VSIFile::readExtendedType(vsi::VSIStream& vsi, vsi::TagInfo& tagInfo) {
    switch (tagInfo.extendedType) {
    case ExtendedType::NEW_VOLUME_HEADER: {
        const int64_t endPointer = vsi.getPos() + tagInfo.dataSize;
        while (vsi.getPos() < endPointer && vsi.getPos() < vsi.getSize()) {
            const int64_t start = vsi.getPos();
            bool ok = readMetadata(vsi, tagInfo);
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
        readMetadata(vsi, tagInfo);
        break;
    }
}

bool vsi::VSIFile::readVolumeHeader(vsi::VSIStream& vsi, vsi::VolumeHeader& volumeHeader) {
    volumeHeader = {};
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
    return true;
}

bool vsi::VSIFile::readMetadata(VSIStream& vsi, TagInfo& parentObject) {
    SLIDEIO_LOG(INFO) << "VSI driver: reading metadata";

    vsi::VolumeHeader volumeHeader;
    const int64_t headerPos = vsi.getPos();

    if (!readVolumeHeader(vsi, volumeHeader))
        return false;

    const uint32_t childCount = volumeHeader.flags & vsi::VOLUME_TAG_COUNT_MASK;
    const int64_t dataFieldOffset = headerPos + volumeHeader.offsetFirstDataField;
    if (dataFieldOffset >= vsi.getSize()) {
        return false;
    }
    vsi.setPos(dataFieldOffset);

    struct vsi::TagHeader tagHeader;
    for (uint tagIndex = 0; tagIndex < childCount; ++tagIndex) {
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
        if (extendedField) {
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
        tagInfo.name = VSITools::getTagName(tagInfo, parentObject);

        if (extendedField) {
            readExtendedType(vsi, tagInfo);
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
            tagInfo.setValue(value);
        }

        parentObject.addChild(tagInfo);

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
