
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
#include "pyramid.hpp"
#include "etsfile.hpp"
#include "vsiscene.hpp"
#include "vsitools.hpp"

using namespace slideio;
namespace fs = boost::filesystem;

namespace slideio
{
    namespace vsi
    {
        struct TempData
        {
            int metadataIndex = -1;
            int previousTag = 0;
        };
    }
}

static bool mapContainsValue(const std::map<std::string, int>& map, int value)
{
    for (const auto& item : map) {
        if (item.second == value) {
            return true;
        }
    }
    return false;
}


vsi::VSIFile::VSIFile(const std::string& filePath) : m_filePath(filePath)
{
    read();
}

void vsi::VSIFile::read()
{
    readVolumeInfo();
    if (m_hasExternalFiles) {
        readExternalFiles();
    }
}

bool vsi::VSIFile::readTags(vsi::VSIStream& vsi, bool populateMetadata, std::string tagPrefix, vsi::TempData& temp)
{
    const int32_t NEW_VOLUME_HEADER = 0;
    const int32_t PROPERTY_SET_VOLUME = 1;
    const int32_t NEW_MDIM_VOLUME_HEADER = 2;

    const int64_t headerPos = vsi.getPos();
    if (headerPos == 403303) {
        int a = 0;
    }
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
    bool dataBlockFollowsHeader = (volumeHeader.flags & vsi::VOLUME_DATA_BLOCK_TYPE_MASK) != 0;
    const uint32_t volumeTagCount = volumeHeader.flags & vsi::VOLUME_TAG_COUNT_MASK;
    vsi.setPos(dataFieldOffset);

    struct vsi::TagHeader tagHeader;
    for (uint tagIndex = 0; tagIndex < volumeTagCount; ++tagIndex) {
        std::string storedValue;
        //auto hdr = vsi.readValue<vsi::TagHeader>();
        vsi.read(tagHeader);
        int32_t nextField = tagHeader.nextField & 0xFFFFFFFFL;
        const bool extraTag = ((tagHeader.fieldType & 0x8000000) >> 27) == 1;
        const bool extendedField = ((tagHeader.fieldType & 0x10000000) >> 28) == 1;
        const bool inlineData = ((tagHeader.fieldType & 0x40000000) >> 30) == 1;
        bool array = (!inlineData && !extendedField) &&
            ((tagHeader.fieldType & 0x20000000) >> 29) == 1;

        int realType = tagHeader.fieldType & 0xffffff;
        int secondTag = -1;

        if (extraTag) {
            vsi.read<int>(secondTag);
        }
        if (tagHeader.tag < 0) {
            if (!inlineData && (tagHeader.dataSize + vsi.getPos()) < vsi.getSize()) {
                vsi.skipBytes(tagHeader.dataSize);
            }
            return false;
        }
        if (tagHeader.tag == vsi::EXTERNAL_FILE_PROPERTIES && temp.previousTag == vsi::IMAGE_FRAME_VOLUME) {
            temp.metadataIndex++;
        }
        else if (tagHeader.tag == vsi::DOCUMENT_PROPERTIES || tagHeader.tag == vsi::SLIDE_PROPERTIES) {
            temp.metadataIndex = -1;
        }

        temp.previousTag = tagHeader.tag;


        if (temp.metadataIndex >= static_cast<int>(m_pyramids.size())) {
            m_pyramids.resize(temp.metadataIndex + 1);
        }

        int32_t tag = tagHeader.tag;
        uint32_t dataSize = tagHeader.dataSize;
        int dimensionTag = -1;
        bool inDimensionProperties = false;
        bool foundChannelTag = false;

        if (extendedField && realType == NEW_VOLUME_HEADER) {
            if (tag == vsi::DIMENSION_DESCRIPTION_VOLUME) {
                dimensionTag = secondTag;
                inDimensionProperties = true;
            }
            int64_t endPointer = vsi.getPos() + dataSize;
            while (vsi.getPos() < endPointer && vsi.getPos() < vsi.getSize())
            {
                int64_t start = vsi.getPos();
                bool ok = readTags(vsi, populateMetadata || inDimensionProperties, 
                    VSITools::getVolumeName(tag), temp);
                if (!ok) {
                    break;
                }
                int64_t end = vsi.getPos();
                if (start >= end) {
                    break;
                }
            }
            if (tag == vsi::DIMENSION_DESCRIPTION_VOLUME) {
                inDimensionProperties = false;
                foundChannelTag = false;
            }
        }
        else if (extendedField && (realType == PROPERTY_SET_VOLUME ||
            realType == NEW_MDIM_VOLUME_HEADER))
        {
            int64_t start = vsi.getPos();
            std::string tagName = realType == NEW_MDIM_VOLUME_HEADER ? VSITools::getVolumeName(tag) : tagPrefix;
            if (tagName.empty() && realType == NEW_MDIM_VOLUME_HEADER) {
                switch (tag) {
                case vsi::Z_START:
                    tagName = "Z start position";
                    break;
                case vsi::Z_INCREMENT:
                    tagName = "Z increment";
                    break;
                case vsi::Z_VALUE:
                    tagName = "Z value";
                    break;
                }
            }
            readTags(vsi, tag != 2037, tagName, temp);
        }
        else {
            std::string tagName = VSITools::getTagName(tag);
            std::string value = inlineData ? std::to_string(dataSize) : " ";


            if (!inlineData && dataSize > 0) {
                switch (realType)
                {
                case vsi::PixelType::CHAR:
                case vsi::PixelType::UCHAR:
                    value = std::to_string(vsi.readValue<uint8_t>());
                    break;
                case vsi::PixelType::SHORT:
                case vsi::PixelType::USHORT:
                    value = std::to_string(vsi.readValue<uint16_t>());
                    break;
                case vsi::PixelType::INT:
                case vsi::PixelType::UINT:
                case vsi::PixelType::DWORD:
                case vsi::PixelType::FIELD_TYPE:
                case vsi::PixelType::MEM_MODEL:
                case vsi::PixelType::COLOR_SPACE:
                    value = std::to_string(vsi.readValue<uint32_t>());
                    break;
                case vsi::PixelType::INT64:
                case vsi::PixelType::UINT64:
                case vsi::PixelType::TIMESTAMP:
                    value = std::to_string(vsi.readValue<uint64_t>());
                    break;
                case vsi::PixelType::FLOAT:
                    value = std::to_string(vsi.readValue<float>());
                    break;
                case vsi::PixelType::DOUBLE:
                case vsi::PixelType::DATE:
                    value = std::to_string(vsi.readValue<double>());
                    break;
                case vsi::PixelType::BOOL:
                    value = std::to_string(vsi.readValue<bool>());
                    break;
                case vsi::PixelType::TCHAR:
                case vsi::PixelType::UNICODE_TCHAR:
                    value = vsi.readString(dataSize);
                    if (temp.metadataIndex >= 0)
                    {
                        vsi::Pyramid& pyramid = m_pyramids[temp.metadataIndex];
                        if (tag == vsi::CHANNEL_NAME) {
                            pyramid.channelNames.push_back(value);
                        }
                        else if (tag == vsi::STACK_NAME && !value.compare("0") == 0 && pyramid.name.empty()) {
                            pyramid.name = value;
                        }
                    }
                    break;
                case vsi::PixelType::VECTOR_INT_2:
                case vsi::PixelType::TUPLE_INT:
                case vsi::PixelType::ARRAY_INT_2:
                case vsi::PixelType::VECTOR_INT_3:
                case vsi::PixelType::ARRAY_INT_3:
                case vsi::PixelType::VECTOR_INT_4:
                case vsi::PixelType::RECT_INT:
                case vsi::PixelType::ARRAY_INT_4:
                case vsi::PixelType::ARRAY_INT_5:
                case vsi::PixelType::DIM_INDEX_1:
                case vsi::PixelType::DIM_INDEX_2:
                case vsi::PixelType::VOLUME_INDEX:
                case vsi::PixelType::PIXEL_INFO_TYPE:
                {
                    uint32_t nIntValues = dataSize / 4;
                    std::vector<int32_t> intValues(nIntValues);
                    if (nIntValues > 1) {
                        value += "(";
                    }
                    for (uint32_t v = 0; v < nIntValues; v++) {
                        intValues[v] = vsi.readValue<int32_t>();
                        value += std::to_string(intValues[v]);
                        if (v < nIntValues - 1) {
                            value += ", ";
                        }
                    }
                    if (nIntValues > 1) {
                        value += ")";
                    }
                    if (temp.metadataIndex >= 0) {
                        vsi::Pyramid& pyramid = m_pyramids[temp.metadataIndex];
                        if (tag == vsi::IMAGE_BOUNDARY) {
                            if (pyramid.width == 0) {
                                pyramid.width = intValues[2];
                                pyramid.height = intValues[3];
                            }
                        }
                        else if (tag == vsi::TILE_ORIGIN) {
                            pyramid.tileOriginX = intValues[0];
                            pyramid.tileOriginY = intValues[1];
                        }
                    }
                }
                break;
                case vsi::PixelType::DOUBLE2:
                case vsi::PixelType::VECTOR_DOUBLE_2:
                case vsi::PixelType::TUPLE_DOUBLE:
                case vsi::PixelType::ARRAY_DOUBLE_2:
                case vsi::PixelType::VECTOR_DOUBLE_3:
                case vsi::PixelType::ARRAY_DOUBLE_3:
                case vsi::PixelType::VECTOR_DOUBLE_4:
                case vsi::PixelType::RECT_DOUBLE:
                case vsi::PixelType::MATRIX_DOUBLE_2_2:
                case vsi::PixelType::MATRIX_DOUBLE_3_3:
                case vsi::PixelType::MATRIX_DOUBLE_4_4:
                {
                    int nDoubleValues = dataSize / sizeof(double);
                    std::vector<double> doubleValues(nDoubleValues);
                    if (nDoubleValues > 1) {
                        value += "(";
                    }
                    for (int v = 0; v < nDoubleValues; v++) {
                        doubleValues[v] = vsi.readValue<double>();
                        value += std::to_string(doubleValues[v]);
                        if (v < nDoubleValues - 1) {
                            value += ", ";
                        }
                    }
                    if (nDoubleValues > 1) {
                        value += ')';
                    }
                    if (temp.metadataIndex >= 0) {
                        vsi::Pyramid& pyramid = m_pyramids[temp.metadataIndex];
                        if (tag == vsi::RWC_FRAME_SCALE) {
                            if (pyramid.physicalSizeX == 0.) {
                                pyramid.physicalSizeX = doubleValues[0];
                                pyramid.physicalSizeY = doubleValues[1];
                            }
                        }
                        else if (tag == vsi::RWC_FRAME_ORIGIN) {
                            if (pyramid.originX == 0.) {
                                pyramid.originX = doubleValues[0];
                                pyramid.originY = doubleValues[1];
                            }
                        }
                    }
                }
                break;
                case vsi::PixelType::RGB:
                {
                    int red = vsi.readValue<uint8_t>();
                    int green = vsi.readValue<uint8_t>();
                    int blue = vsi.readValue<uint8_t>();
                    value = "red = " + std::to_string(red)
                        + ", green = " + std::to_string(green)
                        + ", blue = " + std::to_string(blue);
                }
                break;
                case vsi::PixelType::BGR:
                {
                    int blue = vsi.readValue<uint8_t>();
                    int green = vsi.readValue<uint8_t>();
                    int red = vsi.readValue<uint8_t>();
                    value = "red = " + std::to_string(red)
                        + ", green = " + std::to_string(green)
                        + ", blue = " + std::to_string(blue);
                }
                break;
                }
            }

            if (temp.metadataIndex >= 0) {
                vsi::Pyramid& pyramid = m_pyramids[temp.metadataIndex];
                try {
                    char* end{};
                    if (tag == vsi::STACK_TYPE) {
                        if(pyramid.stackType == vsi::StackType::UNKNOWN) {
                            pyramid.stackType = VSITools::intToStackType(std::stoi(value));
                        }
                        value = VSITools::getStackTypeName(value);
                    }
                    else if (tag == vsi::DEVICE_SUBTYPE) {
                        value = VSITools::getDeviceSubtype(value);
                        pyramid.deviceTypes.push_back(value);
                    }
                    else if (tag == vsi::DEVICE_ID) {
                        pyramid.deviceIDs.push_back(value);
                    }
                    else if (tag == vsi::DEVICE_NAME) {
                        pyramid.deviceNames.push_back(value);
                    }
                    else if (tag == vsi::DEVICE_MANUFACTURER) {
                        pyramid.deviceManufacturers.push_back(value);
                    }
                    else if (tag == vsi::EXPOSURE_TIME && tagPrefix.length() == 0) {
                        pyramid.exposureTimes.push_back(std::stoll(value));
                    }
                    else if (tag == vsi::EXPOSURE_TIME) {
                        pyramid.defaultExposureTime = std::stoll(value);
                        pyramid.otherExposureTimes.push_back(pyramid.defaultExposureTime);
                    }
                    else if (tag == vsi::CREATION_TIME && pyramid.acquisitionTime == 0) {
                        pyramid.acquisitionTime = std::stoll(value);
                    }
                    else if (tag == vsi::REFRACTIVE_INDEX) {
                        pyramid.refractiveIndex = std::strtod(value.c_str(), &end);
                    }
                    else if (tag == vsi::OBJECTIVE_MAG) {
                        pyramid.magnification = std::strtod(value.c_str(), &end);
                    }
                    else if (tag == vsi::NUMERICAL_APERTURE) {
                        pyramid.numericalAperture = std::strtod(value.c_str(), &end);
                    }
                    else if (tag == vsi::WORKING_DISTANCE) {
                        pyramid.workingDistance = std::strtod(value.c_str(), &end);
                    }
                    else if (tag == vsi::OBJECTIVE_NAME) {
                        pyramid.objectiveNames.push_back(value);
                    }
                    else if (tag == vsi::OBJECTIVE_TYPE) {
                        pyramid.objectiveTypes.push_back(std::stol(value));
                    }
                    else if (tag == vsi::BIT_DEPTH) {
                        pyramid.bitDepth = std::stol(value);
                    }
                    else if (tag == vsi::X_BINNING) {
                        pyramid.binningX = std::stol(value);
                    }
                    else if (tag == vsi::Y_BINNING) {
                        pyramid.binningY = std::stol(value);
                    }
                    else if (tag == vsi::CAMERA_GAIN) {
                        pyramid.gain = std::strtod(value.c_str(), &end);
                    }
                    else if (tag == vsi::CAMERA_OFFSET) {
                        pyramid.offset = std::strtod(value.c_str(), &end);
                    }
                    else if (tag == vsi::RED_GAIN) {
                        pyramid.redGain = std::strtod(value.c_str(), &end);
                    }
                    else if (tag == vsi::GREEN_GAIN) {
                        pyramid.greenGain = std::strtod(value.c_str(), &end);
                    }
                    else if (tag == vsi::BLUE_GAIN) {
                        pyramid.blueGain = std::strtod(value.c_str(), &end);
                    }
                    else if (tag == vsi::RED_OFFSET) {
                        pyramid.redOffset = std::strtod(value.c_str(), &end);
                    }
                    else if (tag == vsi::GREEN_OFFSET) {
                        pyramid.greenOffset = std::strtod(value.c_str(), &end);
                    }
                    else if (tag == vsi::BLUE_OFFSET) {
                        pyramid.blueOffset = std::strtod(value.c_str(), &end);
                    }
                    else if (tag == vsi::VALUE) {
                        if (tagPrefix.compare("Channel Wavelength ") == 0) {
                            pyramid.channelWavelengths.push_back(std::strtod(value.c_str(), &end));
                        }
                        else if (tagPrefix.find("Objective Working Distance") == 0) {
                            pyramid.workingDistance = std::strtod(value.c_str(), &end);
                        }
                        else if (tagPrefix.compare("Z start position") == 0) {
                            pyramid.zStart = std::strtod(value.c_str(), &end);
                        }
                        else if (tagPrefix.compare("Z increment") == 0) {
                            pyramid.zIncrement = std::strtod(value.c_str(), &end);
                        }
                        else if (tagPrefix.compare("Z value") == 0) {
                            pyramid.zValues.push_back(std::strtod(value.c_str(), &end));
                        }
                    }
                }
                catch (std::exception e) {
                    SLIDEIO_LOG(INFO) << "Error parsing metadata: " + std::string(e.what());
                }
            }

            if (tag == vsi::DOCUMENT_TIME || tag == vsi::CREATION_TIME) {
                std::ostringstream oss;
                time_t time = std::stoll(value);
                oss << std::put_time(std::localtime(&time), "%d-%m-%Y %H-%M-%S");
                value = oss.str();
            }

            if (tag == vsi::HAS_EXTERNAL_FILE) {
                m_hasExternalFiles = std::stol(value) == 1;
            }

            if (!tagName.empty() && populateMetadata) {
                if (temp.metadataIndex >= 0) {
                    //addMetaList(tagPrefix + tagName, value,
                    //    m_pyramids[metadataIndex].originalMetadata);
                }
                else if (tag != vsi::VALUE || tagPrefix.length() > 0) {
                    // addGlobalMetaList(tagPrefix + tagName, value);
                }
                std::string fullTagName = tagPrefix + tagName;
                if (fullTagName.compare("Channel Wavelength Value") == 0) {
                    m_numChannels++;
                }
                else if (fullTagName.compare("Z valueValue") == 0) {
                    m_numSlices++;
                }
            }
            storedValue = value;
        }
        if (inDimensionProperties) {
            vsi::Pyramid& p = m_pyramids[temp.metadataIndex];
            if (tag == vsi::Z_START && !mapContainsValue(p.dimensionOrdering, dimensionTag)) {
                p.dimensionOrdering["Z"] = dimensionTag;
            }
            else if ((tag == vsi::TIME_START || tag == vsi::DIMENSION_VALUE_ID) &&
                !mapContainsValue(p.dimensionOrdering, dimensionTag))
            {
                p.dimensionOrdering["T"] = dimensionTag;
            }
            else if (tag == vsi::LAMBDA_START &&
                !mapContainsValue(p.dimensionOrdering, dimensionTag))
            {
                p.dimensionOrdering["L"] = dimensionTag;
            }
            else if (tag == vsi::CHANNEL_PROPERTIES && foundChannelTag &&
                !mapContainsValue(p.dimensionOrdering, dimensionTag))
            {
                p.dimensionOrdering["C"] = dimensionTag;
            }
            else if (tag == vsi::CHANNEL_PROPERTIES) {
                foundChannelTag = true;
            }
            else if (tag == vsi::DIMENSION_MEANING && !storedValue.empty()) {
                int dimension = -1;
                try {
                    dimension = std::stoi(storedValue);
                }
                catch (std::exception e) {}
                switch (dimension) {
                case vsi::Z:
                    p.dimensionOrdering["Z"] = dimensionTag;
                    break;
                case vsi::T:
                    p.dimensionOrdering["T"] = dimensionTag;
                    break;
                case vsi::LAMBDA:
                    p.dimensionOrdering["L"] = dimensionTag;
                    break;
                case vsi::C:
                    p.dimensionOrdering["C"] = dimensionTag;
                    break;
                case vsi::PHASE:
                    p.dimensionOrdering["P"] = dimensionTag;
                default:
                    RAISE_RUNTIME_ERROR << "Invalid dimension: " << dimension;
                }
            }
        }

        if (nextField == 0 || tag == -494804095) {
            if (headerPos + dataSize + 32 < vsi.getSize() && headerPos + dataSize >= 0) {
                vsi.setPos(headerPos + dataSize + 32);
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

void vsi::VSIFile::readVolumeInfo()
{
#if defined(WIN32)
    std::wstring filePathW = Tools::toWstring(m_filePath);
    std::ifstream ifs(filePathW, std::ios::binary);
#else
    std::ifstream input(m_filePath, std::ios::binary);
#endif
    vsi::VSIStream vsiStream(ifs);
    vsi::ImageFileHeader header;
    vsiStream.read<vsi::ImageFileHeader>(header);
    if (strncmp((char*)header.magic, "II", 2) != 0) {
        RAISE_RUNTIME_ERROR << "VSI driver: invalid file header. Expected first two bytes: 'II', got: "
            << header.magic;
    }
    if (header.i42 != 42) {
        RAISE_RUNTIME_ERROR << "VSI driver: invalid file header. Expected second word: 42, got: "
            << header.i42;
    }

    vsi::TempData temp;
    readTags(vsiStream, false, "", temp);
}


void vsi::VSIFile::readExternalFiles()
{
    const fs::path filePath(m_filePath);
    const fs::path dirPath = filePath.parent_path();
    const fs::path fileName = filePath.filename();
    const fs::path subDirName = "_" + fileName.stem().string() + "_";
    const fs::path subDirPath = dirPath / subDirName;
    if (fs::exists(subDirPath)) {
        std::list<std::string> files = Tools::findFilesWithExtension(subDirPath.string(), ".ets");
        for (auto file : files) {
            auto etsFile = std::make_shared<vsi::EtsFile>(file);
            etsFile->read();
            m_etsFiles.push_back(etsFile);
        }
    }
}

