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
    for (auto part = path.rbegin(); part != path.rend(); part++) {
        if (count > 0) {
            dirName = part->string();
            break;
        }
        count++;
    }
    int suffix = std::stoi(dirName.c_str() + 5); // skip 'stack' prefix
    return suffix;
}


static std::vector<std::string> splitVectorValues(const std::string& value) {
    std::string val = value;
    val.erase(std::remove(val.begin(), val.end(), '('), val.end());
    val.erase(std::remove(val.begin(), val.end(), ')'), val.end());
    std::vector<std::string> tokens;
    boost::split(tokens, val, boost::is_any_of(","));
    return tokens;
}

std::string slideio::vsi::getStackTypeName(StackType type) {
    switch (type) {
    case StackType::DEFAULT_IMAGE:
        return "Image";
    case StackType::OVERVIEW_IMAGE:
        return "Overview";
    case StackType::SAMPLE_MASK:
        return "Sample mask";
    case StackType::FOCUS_IMAGE:
        return "Focus image";
    case StackType::EFI_SHARPNESS_MAP:
        return "EFI sharpness map";
    case StackType::EFI_HEIGHT_MAP:
        return "EFI height map";
    case StackType::EFI_TEXTURE_MAP:
        return "EFI texture map";
    case StackType::EFI_STACK:
        return "EFI stack";
    case StackType::MACRO_IMAGE:
        return "Macro image";
    case StackType::UNKNOWN:
        return "Unknown";
    }
    return "Unknown";   
}

void VSIFile::extractVolumesFromMetadata() {
    const std::vector<int> REL_PATH_TO_MAGNIFICATION = {
        Tag::MICROSCOPE,
        Tag::MICROSCOPE_PROPERTIES,
        Tag::OPTICAL_PROPERTIES,
        Tag::OBJECTIVE_MAG
    };

    const std::vector<int> REL_PATH_TO_BITDEPTH = {
        Tag::MICROSCOPE,
        Tag::MICROSCOPE_PROPERTIES,
        Tag::OPTICAL_PROPERTIES,
        Tag::BIT_DEPTH
    };

    int countExternalVolumes = 0;
    int countInternalVolumes = 0;
    int countSkippedVolumes = 0;
    if (!m_metadata.empty()) {
        std::list<const TagInfo*> volumes;
        getVolumeMetadataItems(volumes);
        for (const TagInfo* const& volume : volumes) {
            const StackType stackType = getVolumeStackType(volume);
            //if (stackType != StackType::DEFAULT_IMAGE && stackType != StackType::OVERVIEW_IMAGE) {
            //    continue;
            //}
            std::shared_ptr<Volume> volumeObj = std::make_shared<Volume>();
            volumeObj->setType(stackType);
            std::list<const TagInfo*> frames;
            getImageFrameMetadataItems(volume, frames);
            if (frames.size() > 1) {
                RAISE_RUNTIME_ERROR << "VSI Unexpected stack with multiple external files";
            }
            if (!frames.empty()) {
                const TagInfo* frame = frames.front();
                const TagInfo* externalFile = frame->findChild(Tag::EXTERNAL_FILE_PROPERTIES);
                const TagInfo* stackProps = volume->findChild(Tag::MULTIDIM_STACK_PROPERTIES);
                const TagInfo* color = volume->findChild(Tag::DEFAULT_BACKGROUND_COLOR);
                const TagInfo* ifdTag = frame->findChild(Tag::DEFAULT_SAMPLE_PIXEL_DATA_IFD);
                if (ifdTag) {
                    volumeObj->setIFD(ifdTag->secondTag);
                }
                if (color) {
                    try {
                        volumeObj->setDefaultColor(std::stoi(color->value));
                    }
                    catch (std::exception& ex) {
                        SLIDEIO_LOG(WARNING) << "VSI driver: error reading default color (ignored): " << ex.what();
                    }
                }
                if (stackProps) {
                    const TagInfo* stackName = stackProps->findChild(Tag::STACK_NAME);
                    if (stackName) {
                        volumeObj->setName(stackName->value);
                    }
                    const TagInfo* stackType = stackProps->findChild(Tag::STACK_TYPE);
                    if (stackType) {
                        try {
                            const int val = std::stoi(stackType->value);
                            volumeObj->setType(VSITools::intToStackType(val));
                        }
                        catch (std::exception& ex) {
                            SLIDEIO_LOG(WARNING) << "VSI driver: error reading stack type (ignored): " << ex.what();
                        }
                    }
                    const TagInfo* resolutionTag = stackProps->findChild(Tag::RWC_FRAME_SCALE);
                    if (resolutionTag) {
                        std::vector<std::string> tokens = splitVectorValues(resolutionTag->value);
                        if (tokens.size() != 2) {
                            RAISE_RUNTIME_ERROR << "VSI driver: invalid number of image resolutions. Expected: 2, Received: "
                                << tokens.size() << " " << resolutionTag->value;
                        }
                        try {
                            const double xRes = 1.e-6 * std::stod(tokens[0]);
                            const double yRes = 1.e-6 * std::stod(tokens[1]);
                            volumeObj->setResolution(Resolution(xRes, yRes));
                        }
                        catch (std::exception& ex) {
                            SLIDEIO_LOG(WARNING) << "VSI driver: error reading resolution (ignored): " << ex.what();
                        }
                    }
                    if(externalFile) {
                        const TagInfo* boundary = externalFile->findChild(Tag::IMAGE_BOUNDARY);
                        if (boundary) {
                            std::vector<std::string> tokens = splitVectorValues(boundary->value);
                            if (tokens.size() != 4) {
                                RAISE_RUNTIME_ERROR << "VSI driver: invalid number of image boundaries. Expected: 4, Received: "
                                    << tokens.size() << " " << boundary->value;
                            }
                            try {
                                const int width = std::stoi(tokens[2]);
                                const int height = std::stoi(tokens[3]);
                                volumeObj->setSize(cv::Size(width, height));
                            }
                            catch (std::exception& ex) {
                                RAISE_RUNTIME_ERROR << "VSI driver: error reading image boundary: " << ex.what();
                            }
                        }
                    }
                    const TagInfo* magnification = stackProps->findChild(REL_PATH_TO_MAGNIFICATION);
                    if (magnification) {
                        try {
                            volumeObj->setMagnification(std::stod(magnification->value));
                        }
                        catch (std::exception& ex) {
                            SLIDEIO_LOG(WARNING) << "VSI driver: error reading magnification (ignored): " << ex.what();
                        }
                    }
                    const TagInfo* bitDepth = stackProps->findChild(REL_PATH_TO_BITDEPTH);
                    if (bitDepth) {
                        try {
                            volumeObj->setBitDepth(std::stoi(bitDepth->value));
                        }
                        catch (std::exception& ex) {
                            SLIDEIO_LOG(WARNING) << "VSI driver: error reading bit depth (ignored): " << ex.what();
                        }
                    }
                }
                for (TagInfo::const_iterator it = volume->begin(); it != volume->end(); ++it) {
                    it = volume->findNextChild(Tag::DIMENSION_DESCRIPTION_VOLUME, it);
                    if (it == volume->end()) {
                        break;
                    }
                    const TagInfo& dimensionDescription = *it;
                    const int index = dimensionDescription.secondTag;
                    for (auto itc = dimensionDescription.children.begin(); itc != dimensionDescription.children.
                         end(); ++itc) {
                        if (itc->tag == Tag::DIMENSION_PARAMETERS) {
                            auto& dimParams = *itc;
                            const TagInfo* indexTag = dimParams.findChild(Tag::DIMENSION_INDEX);
                            if (indexTag) {
                                int dimension = std::stoi(indexTag->value);
                                switch (dimension) {
                                case 1: {
                                    volumeObj->setDimensionOrder(Dimensions::Z, index + 2);
                                    const TagInfo* channelInfo = itc->findChild(Tag::CHANNEL_INFO_PROPERTIES);
                                    if(channelInfo) {
                                        const TagInfo* valueTag = channelInfo->findChild(Tag::VALUE);
                                        if(valueTag) {
                                            double zRes = std::stod(valueTag->value);
                                            volumeObj->setZResolution(zRes*1.e-6);
                                        }
                                    }
                                    break;
                                }
                                case 2: {
                                    volumeObj->setDimensionOrder(Dimensions::T, index + 2);
                                    const TagInfo* channelInfo = itc->findChild(Tag::CHANNEL_INFO_PROPERTIES);
                                    if (channelInfo) {
                                        const TagInfo* valueTag = channelInfo->findChild(Tag::VALUE);
                                        if (valueTag) {
                                            double res = std::stod(valueTag->value);
                                            volumeObj->setTResolution(res);
                                        }
                                    }
                                    break;
                                }
                                case 3:
                                    volumeObj->setDimensionOrder(Dimensions::L, index + 2);
                                    break;
                                case 4:
                                    volumeObj->setDimensionOrder(Dimensions::C, index + 2);
                                    break;
                                case 9:
                                    volumeObj->setDimensionOrder(Dimensions::P, index + 2);
                                    break;
                                default:
                                    RAISE_RUNTIME_ERROR << "VSI driver: invalid dimension index: " << dimension;
                                    break;
                                }
                            }
                            if(itc->secondTag>=0) {
                                auto channelName = itc->findChild(Tag::CHANNEL_NAME);
                                if(channelName) {
                                    volumeObj->setChannelName(itc->secondTag, channelName->value);
                                }
                            }
                        }
                    }
                }
            }
            m_volumes.push_back(volumeObj);
        }
    }
}

VSIFile::VSIFile(const std::string& filePath) : m_filePath(filePath) {
    m_metadata.tag = Tag::ROOT;
    m_metadata.name = "root";
    read();
}

std::string VSIFile::getRawMetadata() const {
    boost::json::object jsonMtd;
    serializeMetadata(m_metadata, jsonMtd);
    return boost::json::serialize(jsonMtd);
}


void VSIFile::assignAuxVolumes() {
    try {
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
    const TagInfo* volumeCollection = m_metadata.findChild(Tag::COLLECTION_VOLUME);
    if (volumeCollection) {
        for (auto itPos = volumeCollection->begin(); itPos != volumeCollection->end(); ++itPos) {
            auto itVolume = volumeCollection->findNextChild(Tag::MULTIDIM_IMAGE_VOLUME, itPos);
            if (itVolume == volumeCollection->end())
                break;
            volumes.push_back(&*itVolume);
            itPos = itVolume;
        }
    }
}

void VSIFile::getImageFrameMetadataItems(const TagInfo* volume, std::list<const TagInfo*>& frames) {
    for (auto itPos = volume->begin(); itPos != volume->end(); ++itPos) {
        auto itFrame = volume->findNextChild(Tag::IMAGE_FRAME_VOLUME, itPos);
        if (itFrame == volume->end())
            break;
        if (itFrame->findChild(Tag::EXTERNAL_FILE_PROPERTIES)
            || itFrame->findChild(Tag::DEFAULT_SAMPLE_PIXEL_DATA_IFD)) {
            // ignore all frames without external file
            // they contain undocumented vector layers
            frames.push_back(&*itFrame);
        }
        itPos = itFrame;
    }
}

void VSIFile::read() {
    SLIDEIO_LOG(INFO) << "VSI driver: reading file " << m_filePath;
    readVolumeInfo();
    checkExternalFilePresence();
    extractVolumesFromMetadata();
    TiffTools::scanFile(m_filePath, m_directories);
    assignAuxVolumes();
    if (m_expectExternalFiles) {
        readExternalFiles();
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

void VSIFile::checkExternalFilePresence() {
    SLIDEIO_LOG(INFO) << "VSI driver: checking external file presence";
    std::list<const TagInfo*> volumes;
    getVolumeMetadataItems(volumes);
    for (auto& volume : volumes) {
        std::list<const TagInfo*> frames;
        getImageFrameMetadataItems(volume, frames);
        for (auto& frame : frames) {
            const TagInfo* externalFile = frame->findChild(Tag::EXTERNAL_FILE_PROPERTIES);
            if (externalFile) {
                const TagInfo* hasExternalFile = externalFile->findChild(Tag::HAS_EXTERNAL_FILE);
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

StackType VSIFile::getVolumeStackType(const TagInfo* volume) {
    const TagInfo* stackProps = volume->findChild(Tag::MULTIDIM_STACK_PROPERTIES);
    if (stackProps) {
        const TagInfo* stackType = stackProps->findChild(Tag::STACK_TYPE);
        if (stackType) {
            const int val = std::stoi(stackType->value);
            return VSITools::intToStackType(val);
        }
        else {
            return StackType::DEFAULT_IMAGE;
        }
    }
    return StackType::UNKNOWN;
}


void VSIFile::serializeMetadata(const TagInfo& tagInfo, boost::json::object& jsonObj) const {
    jsonObj["tag"] = tagInfo.tag;
    jsonObj["name"] = tagInfo.name;
    jsonObj["value"] = tagInfo.value;
    jsonObj["secondTag"] = tagInfo.secondTag;
    if (!tagInfo.children.empty()) {
        boost::json::array array;
        for (const auto& child : tagInfo.children) {
            boost::json::object childObject;
            serializeMetadata(child, childObject);
            array.emplace_back(std::move(childObject));
        }
        jsonObj["value"] = std::move(array);
    }
    else {
        for (const auto& child : tagInfo.children) {
            boost::json::object childObject;
            serializeMetadata(child, childObject);
            std::string tag = std::to_string(child.tag);
            jsonObj[tag] = std::move(childObject);
        }
    }
}

void VSIFile::readVolumeInfo() {
    SLIDEIO_LOG(INFO) << "VSI driver: reading volume info";
    VSIStream vsiStream(m_filePath);
    ImageFileHeader header;
    vsiStream.read<ImageFileHeader>(header);
    if (strncmp(reinterpret_cast<char*>(header.magic), "II", 2) != 0) {
        RAISE_RUNTIME_ERROR << "VSI driver: invalid file header. Expected first two bytes: 'II', got: "
            << header.magic;
    }
    if (header.i42 != 42) {
        RAISE_RUNTIME_ERROR << "VSI driver: invalid file header. Expected second word: 42, got: "
            << header.i42;
    }
    std::list<TagInfo> path = {m_metadata};
    readMetadata(vsiStream, path);
    if (path.size() != 1) {
        RAISE_RUNTIME_ERROR << "VSI driver: invalid metadata path";
    }
    m_metadata = path.front();
    {
        //boost::json::object root;
        //serializeMetadata(m_metadata, root);
        //std::ofstream ofs("d:\\Temp\\metadata.json");
        //ofs << boost::json::serialize(root);
        //ofs.close();
    }
    checkExternalFilePresence();
}


void VSIFile::readExternalFiles() {
    SLIDEIO_LOG(INFO) << "VSI driver: reading external ETS files";
    const fs::path filePath(m_filePath);
    const fs::path dirPath = filePath.parent_path();
    const fs::path fileName = filePath.filename();
    const fs::path subDirName = "_" + fileName.stem().string() + "_";
    const fs::path subDirPath = dirPath / subDirName;
    if (fs::exists(subDirPath)) {
        const std::list<std::string> files = Tools::findFilesWithExtension(subDirPath.string(), ".ets");
        if (files.size() != this->getNumVolumes()) {
            SLIDEIO_LOG(WARNING) << "VSI driver: number of ETS files does not match the number of volumes";
        }
        int index = 0;
        std::list<std::shared_ptr<Volume>> volumes(m_volumes.begin(), m_volumes.end());
        for (const auto& file : files) {
            auto etsFile = std::make_shared<EtsFile>(file);
            etsFile->read(volumes);
            m_etsFiles.push_back(etsFile);
        }
    }
    std::sort(m_etsFiles.begin(), m_etsFiles.end(),
        [](const std::shared_ptr<EtsFile>& left, const std::shared_ptr<EtsFile>& right) {
            const int leftStackId = extractBaseDirectoryNameSuffix(left->getFilePath());
            const int rightStackId = extractBaseDirectoryNameSuffix(right->getFilePath());
            return leftStackId < rightStackId;
        });
}


void VSIFile::readExtendedType(VSIStream& vsi, TagInfo& tagInfo, std::list<TagInfo>& path) {
    switch (tagInfo.extendedType) {
    case ExtendedType::NEW_VOLUME_HEADER: {
        const int64_t endPointer = vsi.getPos() + tagInfo.dataSize;
        path.push_back(tagInfo);
        while (vsi.getPos() < endPointer && vsi.getPos() < vsi.getSize()) {
            const int64_t start = vsi.getPos();
            bool ok = readMetadata(vsi, path);
            if (!ok) {
                break;
            }
            const int64_t end = vsi.getPos();
            if (start >= end) {
                break;
            }
        }
        tagInfo = path.back();
        path.pop_back();
    }
    break;
    case ExtendedType::PROPERTY_SET_VOLUME:
    case ExtendedType::NEW_MDIM_VOLUME_HEADER:
        path.push_back(tagInfo);
        readMetadata(vsi, path);
        tagInfo = path.back();
        path.pop_back();
        break;
    }
}

bool VSIFile::readVolumeHeader(VSIStream& vsi, VolumeHeader& volumeHeader) {
    volumeHeader = {};
    vsi.read<VolumeHeader>(volumeHeader);
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

bool VSIFile::readMetadata(VSIStream& vsi, std::list<TagInfo>& path) {
    //SLIDEIO_LOG(INFO) << "VSI driver: reading metadata";
    auto& parentObject = path.back();
    VolumeHeader volumeHeader;
    const int64_t headerPos = vsi.getPos();

    if (!readVolumeHeader(vsi, volumeHeader))
        return false;

    const uint32_t childCount = volumeHeader.flags & VOLUME_TAG_COUNT_MASK;
    const int64_t dataFieldOffset = headerPos + volumeHeader.offsetFirstDataField;
    if (dataFieldOffset >= vsi.getSize()) {
        return false;
    }
    vsi.setPos(dataFieldOffset);

    struct TagHeader tagHeader;
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
        tagInfo.name = VSITools::getTagName(tagInfo, path);

        if (extendedField) {
            readExtendedType(vsi, tagInfo, path);
        }
        else {
            std::string value = inlineData ? std::to_string(tagInfo.dataSize) : " ";
            if (!inlineData && tagInfo.dataSize > 0) {
                value = VSITools::extractTagValue(vsi, tagInfo);
            }

            if (tagInfo.tag == Tag::DOCUMENT_TIME || tagInfo.tag == Tag::CREATION_TIME) {
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
