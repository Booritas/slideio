// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/vsi/vsislide.hpp"
#include <fstream>
#include <cstdlib>
#include <iomanip>

#include "slideio/core/tools/tools.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/drivers/vsi/vsistruct.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/imagetools/libtiff.hpp"
#include "slideio/drivers/vsi/vsitags.hpp"


using namespace slideio;

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


VSISlide::VSISlide(const std::string& filePath) : m_filePath(filePath)
{
    init();
}

bool VSISlide::readTags(vsi::VSIStream& vsi, bool populateMetadata, std::string tagPrefix, vsi::TempData& temp)
{
    const int32_t NEW_VOLUME_HEADER = 0;
    const int32_t PROPERTY_SET_VOLUME = 1;
    const int32_t NEW_MDIM_VOLUME_HEADER = 2;

    const int64_t headerPos = vsi.getPos();
    if(headerPos == 403303) {
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
    if(dataFieldOffset>=vsi.getSize()) {
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
                bool ok = readTags(vsi, populateMetadata || inDimensionProperties, getVolumeName(tag), temp);
                if(!ok) {
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
            std::string tagName = realType == NEW_MDIM_VOLUME_HEADER ? getVolumeName(tag) : tagPrefix;
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
            std::string tagName = getTagName(tag);
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
                    if(temp.metadataIndex>=0)
                    {
                        vsi::Pyramid& pyramid = m_pyramids[temp.metadataIndex];
                        if (tag == vsi::CHANNEL_NAME) {
                            pyramid.channelNames.push_back(value);
                        }
                        else if (tag == vsi::STACK_NAME && !value.compare("0")==0 && pyramid.name.empty()) {
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
                        vsi::Pyramid pyramid = m_pyramids[temp.metadataIndex];
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
                    if(temp.metadataIndex>=0) {
                        vsi::Pyramid pyramid = m_pyramids[temp.metadataIndex];
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
                vsi::Pyramid pyramid = m_pyramids[temp.metadataIndex];
                try {
                    char* end{};
                    if (tag == vsi::STACK_TYPE) {
                        value = getStackType(value);
                    }
                    else if (tag == vsi::DEVICE_SUBTYPE) {
                        value = getDeviceSubtype(value);
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
                        if (tagPrefix.compare("Channel Wavelength ")==0) {
                            pyramid.channelWavelengths.push_back(std::strtod(value.c_str(), &end));
                        }
                        else if (tagPrefix.find("Objective Working Distance")==0) {
                            pyramid.workingDistance = std::strtod(value.c_str(), &end);
                        }
                        else if (tagPrefix.compare("Z start position")==0) {
                            pyramid.zStart = std::strtod(value.c_str(), &end);
                        }
                        else if (tagPrefix.compare("Z increment")==0) {
                            pyramid.zIncrement = std::strtod(value.c_str(), &end);
                        }
                        else if (tagPrefix.compare("Z value")==0) {
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
                expectETS = std::stol(value) == 1;
            }

            if (!tagName.empty() && populateMetadata) {
                if (temp.metadataIndex >= 0) {
                    //addMetaList(tagPrefix + tagName, value,
                    //    m_pyramids[metadataIndex].originalMetadata);
                }
                else if (tag != vsi::VALUE || tagPrefix.length() > 0) {
                    // addGlobalMetaList(tagPrefix + tagName, value);
                }
                if (std::string("Channel Wavelength Value").compare(tagPrefix + tagName)) {
                    channelCount++;
                }
                else if (std::string("Z valueValue").compare(tagPrefix + tagName)==0) {
                    zCount++;
                }
            }
            storedValue = value;
        }
        if (inDimensionProperties) {
            vsi::Pyramid p = m_pyramids[temp.metadataIndex];
            if (tag == vsi::Z_START && !mapContainsValue(p.dimensionOrdering, dimensionTag)) {
                p.dimensionOrdering["Z"] = dimensionTag;
            }
            else if ((tag == vsi::TIME_START || tag == vsi::DIMENSION_VALUE_ID) &&
                !mapContainsValue(p.dimensionOrdering,dimensionTag))
            {
                p.dimensionOrdering["T"] = dimensionTag;
            }
            else if (tag == vsi::LAMBDA_START &&
                !mapContainsValue(p.dimensionOrdering,dimensionTag))
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
                    p.dimensionOrdering["T"] =  dimensionTag;
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
                    RAISE_RUNTIME_ERROR << "Invalid dimension: "  << dimension;
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

void VSISlide::readVolumeInfo()
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

void VSISlide::init()
{
    /*
    TIFFKeeper file(m_filePath, true);
    const int numDirs = libtiff::TIFFNumberOfDirectories(file);
    std::string external = file.readStringTag(2018);
    */


    readVolumeInfo();
}

std::string VSISlide::getVolumeName(int32_t tag)
{
    switch (tag)
    {
        case vsi::COLLECTION_VOLUME:
        case vsi::MULTIDIM_IMAGE_VOLUME:
        case vsi::IMAGE_FRAME_VOLUME:
        case vsi::DIMENSION_SIZE:
        case vsi::IMAGE_COLLECTION_PROPERTIES:
        case vsi::MULTIDIM_STACK_PROPERTIES:
        case vsi::FRAME_PROPERTIES:
        case vsi::DIMENSION_DESCRIPTION_VOLUME:
        case vsi::CHANNEL_PROPERTIES:
        case vsi::DISPLAY_MAPPING_VOLUME:
        case vsi::LAYER_INFO_PROPERTIES:
            return "";
        case vsi::OPTICAL_PATH:
            return "Microscope ";
        case 2417:
            return "Channel Wavelength ";
        case vsi::WORKING_DISTANCE:
            return "Objective Working Distance ";
    }
    return "";

}

std::string VSISlide::getTagName(int32_t tag)
{
    switch (tag) {
    case vsi::Y_PLANE_DIMENSION_UNIT:
        return "Image plane rectangle unit (Y dimension)";
    case vsi::Y_DIMENSION_UNIT:
        return "Y dimension unit";
    case vsi::CHANNEL_OVERFLOW:
        return "Channel under/overflow";
    case vsi::SLIDE_SPECIMEN:
        return "Specimen";
    case vsi::SLIDE_TISSUE:
        return "Tissue";
    case vsi::SLIDE_PREPARATION:
        return "Preparation";
    case vsi::SLIDE_STAINING:
        return "Staining";
    case vsi::SLIDE_INFO:
        return "Slide Info";
    case vsi::SLIDE_NAME:
        return "Slide Name";
    case vsi::EXPOSURE_TIME:
        return "Exposure time (microseconds)";
    case vsi::CAMERA_GAIN:
        return "Camera gain";
    case vsi::CAMERA_OFFSET:
        return "Camera offset";
    case vsi::CAMERA_GAMMA:
        return "Gamma";
    case vsi::SHARPNESS:
        return "Sharpness";
    case vsi::RED_GAIN:
        return "Red channel gain";
    case vsi::GREEN_GAIN:
        return "Green channel gain";
    case vsi::BLUE_GAIN:
        return "Blue channel gain";
    case vsi::RED_OFFSET:
        return "Red channel offset";
    case vsi::GREEN_OFFSET:
        return "Green channel offset";
    case vsi::BLUE_OFFSET:
        return "Blue channel offset";
    case vsi::SHADING_SUB:
        return "Shading sub";
    case vsi::SHADING_MUL:
        return "Shading mul";
    case vsi::X_BINNING:
        return "Binning (X)";
    case vsi::Y_BINNING:
        return "Binning (Y)";
    case vsi::CLIPPING:
        return "Clipping";
    case vsi::MIRROR_H:
        return "Mirror (horizontal)";
    case vsi::MIRROR_V:
        return "Mirror (vertical)";
    case vsi::CLIPPING_STATE:
        return "Clipping state";
    case vsi::ICC_ENABLED:
        return "ICC enabled";
    case vsi::BRIGHTNESS:
        return "Brightness";
    case vsi::CONTRAST:
        return "Contrast";
    case vsi::CONTRAST_TARGET:
        return "Contrast reference";
    case vsi::ACCUMULATION:
        return "Camera accumulation";
    case vsi::AVERAGING:
        return "Camera averaging";
    case vsi::ISO_SENSITIVITY:
        return "ISO sensitivity";
    case vsi::ACCUMULATION_MODE:
        return "Camera accumulation mode";
    case vsi::AUTOEXPOSURE:
        return "Autoexposure enabled";
    case vsi::EXPOSURE_METERING_MODE:
        return "Autoexposure metering mode";
    case vsi::Z_START:
        return "Z stack start";
    case vsi::Z_INCREMENT:
        return "Z stack increment";
    case vsi::Z_VALUE:
        return "Z position";
    case vsi::TIME_START:
        return "Timelapse start";
    case vsi::TIME_INCREMENT:
        return "Timelapse increment";
    case vsi::TIME_VALUE:
        return "Timestamp";
    case vsi::LAMBDA_START:
        return "Lambda start";
    case vsi::LAMBDA_INCREMENT:
        return "Lambda increment";
    case vsi::LAMBDA_VALUE:
        return "Lambda value";
    case vsi::DIMENSION_NAME:
        return "Dimension name";
    case vsi::DIMENSION_MEANING:
        return "Dimension description";
    case vsi::DIMENSION_START_ID:
        return "Dimension start ID";
    case vsi::DIMENSION_INCREMENT_ID:
        return "Dimension increment ID";
    case vsi::DIMENSION_VALUE_ID:
        return "Dimension value ID";
    case vsi::IMAGE_BOUNDARY:
        return "Image size";
    case vsi::TILE_SYSTEM:
        return "Tile system";
    case vsi::HAS_EXTERNAL_FILE:
        return "External file present";
    case vsi::EXTERNAL_DATA_VOLUME:
        return "External file volume";
    case vsi::TILE_ORIGIN:
        return "Origin of tile coordinate system";
    case vsi::DISPLAY_LIMITS:
        return "Display limits";
    case vsi::STACK_DISPLAY_LUT:
        return "Stack display LUT";
    case vsi::GAMMA_CORRECTION:
        return "Gamma correction";
    case vsi::FRAME_ORIGIN:
        return "Frame origin (plane coordinates)";
    case vsi::FRAME_SCALE:
        return "Frame scale (plane coordinates)";
    case vsi::DISPLAY_COLOR:
        return "Display color";
    case vsi::CREATION_TIME:
        return "Creation time (UTC)";
    case vsi::RWC_FRAME_ORIGIN:
        return "Origin";
    case vsi::RWC_FRAME_SCALE:
        return "Calibration";
    case vsi::RWC_FRAME_UNIT:
        return "Calibration units";
    case vsi::STACK_NAME:
        return "Layer";
    case vsi::CHANNEL_DIM:
        return "Channel dimension";
    case vsi::STACK_TYPE:
        return "Image Type";
    case vsi::LIVE_OVERFLOW:
        return "Live overflow";
    case vsi::IS_TRANSMISSION:
        return "IS transmission mask";
    case vsi::CONTRAST_BRIGHTNESS:
        return "Contrast and brightness";
    case vsi::ACQUISITION_PROPERTIES:
        return "Acquisition properties";
    case vsi::GRADIENT_LUT:
        return "Gradient LUT";
    case vsi::DISPLAY_PROCESSOR_TYPE:
        return "Display processor type";
    case vsi::RENDER_OPERATION_ID:
        return "Render operation ID";
    case vsi::DISPLAY_STACK_ID:
        return "Displayed stack ID";
    case vsi::TRANSPARENCY_ID:
        return "Transparency ID";
    case vsi::THIRD_ID:
        return "Display third ID";
    case vsi::DISPLAY_VISIBLE:
        return "Display visible";
    case vsi::TRANSPARENCY_VALUE:
        return "Transparency value";
    case vsi::DISPLAY_LUT:
        return "Display LUT";
    case vsi::DISPLAY_STACK_INDEX:
        return "Display stack index";
    case vsi::CHANNEL_TRANSPARENCY_VALUE:
        return "Channel transparency value";
    case vsi::CHANNEL_VISIBLE:
        return "Channel visible";
    case vsi::SELECTED_CHANNELS:
        return "List of selected channels";
    case vsi::DISPLAY_GAMMA_CORRECTION:
        return "Display gamma correction";
    case vsi::CHANNEL_GAMMA_CORRECTION:
        return "Channel gamma correction";
    case vsi::DISPLAY_CONTRAST_BRIGHTNESS:
        return "Display contrast and brightness";
    case vsi::CHANNEL_CONTRAST_BRIGHTNESS:
        return "Channel contrast and brightness";
    case vsi::ACTIVE_STACK_DIMENSION:
        return "Active stack dimension";
    case vsi::SELECTED_FRAMES:
        return "Selected frames";
    case vsi::DISPLAYED_LUT_ID:
        return "Displayed LUT ID";
    case vsi::HIDDEN_LAYER:
        return "Hidden layer";
    case vsi::LAYER_XY_FIXED:
        return "Layer fixed in XY";
    case vsi::ACTIVE_LAYER_VECTOR:
        return "Active layer vector";
    case vsi::ACTIVE_LAYER_INDEX_VECTOR:
        return "Active layer index vector";
    case vsi::CHAINED_LAYERS:
        return "Chained layers";
    case vsi::LAYER_SELECTION:
        return "Layer selection";
    case vsi::LAYER_SELECTION_INDEX:
        return "Layer selection index";
    case vsi::CANVAS_COLOR_1:
        return "Canvas background color 1";
    case vsi::CANVAS_COLOR_2:
        return "Canvas background color 2";
    case vsi::ORIGINAL_FRAME_RATE:
        return "Original frame rate (ms)";
    case vsi::USE_ORIGINAL_FRAME_RATE:
        return "Use original frame rate";
    case vsi::ACTIVE_CHANNEL:
        return "Active channel";
    case vsi::PLANE_UNIT:
        return "Plane unit";
    case vsi::PLANE_ORIGIN_RWC:
        return "Origin";
    case vsi::PLANE_SCALE_RWC:
        return "Physical pixel size";
    case vsi::MAGNIFICATION:
        return "Original magnification";
    case vsi::DOCUMENT_NAME:
        return "Document Name";
    case vsi::DOCUMENT_NOTE:
        return "Document Note";
    case vsi::DOCUMENT_TIME:
        return "Document Creation Time";
    case vsi::DOCUMENT_AUTHOR:
        return "Document Author";
    case vsi::DOCUMENT_COMPANY:
        return "Document Company";
    case vsi::DOCUMENT_CREATOR_NAME:
        return "Document creator name";
    case vsi::DOCUMENT_CREATOR_MAJOR_VERSION:
        return "Document creator major version";
    case vsi::DOCUMENT_CREATOR_MINOR_VERSION:
        return "Document creator minor version";
    case vsi::DOCUMENT_CREATOR_SUB_VERSION:
        return "Document creator sub version";
    case vsi::DOCUMENT_CREATOR_BUILD_NUMBER:
        return "Product Build Number";
    case vsi::DOCUMENT_CREATOR_PACKAGE:
        return "Document creator package";
    case vsi::DOCUMENT_PRODUCT:
        return "Document product";
    case vsi::DOCUMENT_PRODUCT_NAME:
        return "Document product name";
    case vsi::DOCUMENT_PRODUCT_VERSION:
        return "Document product version";
    case vsi::DOCUMENT_TYPE_HINT:
        return "Document type hint";
    case vsi::DOCUMENT_THUMB:
        return "Document thumbnail";
    case vsi::COARSE_PYRAMID_LEVEL:
        return "Coarse pyramid level";
    case vsi::EXTRA_SAMPLES:
        return "Extra samples";
    case vsi::DEFAULT_BACKGROUND_COLOR:
        return "Default background color";
    case vsi::VERSION_NUMBER:
        return "Version number";
    case vsi::CHANNEL_NAME:
        return "Channel name";
    case vsi::OBJECTIVE_MAG:
        return "Magnification";
    case vsi::NUMERICAL_APERTURE:
        return "Numerical Aperture";
    case vsi::WORKING_DISTANCE:
        return "Objective Working Distance";
    case vsi::OBJECTIVE_NAME:
        return "Objective Name";
    case vsi::OBJECTIVE_TYPE:
        return "Objective Type";
    case 120065:
        return "Objective Description";
    case 120066:
        return "Objective Subtype";
    case 120069:
        return "Brightness Correction";
    case 120070:
        return "Objective Lens";
    case 120075:
        return "Objective X Shift";
    case 120076:
        return "Objective Y Shift";
    case 120077:
        return "Objective Z Shift";
    case 120078:
        return "Objective Gear Setting";
    case 120635:
        return "Slide Bar Code";
    case 120638:
        return "Tray No.";
    case 120637:
        return "Slide No.";
    case 34:
        return "Product Name";
    case 35:
        return "Product Version";
    case vsi::DEVICE_NAME:
        return "Device Name";
    case vsi::BIT_DEPTH:
        return "Camera Actual Bit Depth";
    case 120001:
        return "Device Position";
    case 120050:
        return "TV Adapter Magnification";
    case vsi::REFRACTIVE_INDEX:
        return "Objective Refractive Index";
    case 120117:
        return "Device Type";
    case vsi::DEVICE_ID:
        return "Device Unit ID";
    case vsi::DEVICE_SUBTYPE:
        return "Device Subtype";
    case 120132:
        return "Device Model";
    case vsi::DEVICE_MANUFACTURER:
        return "Device Manufacturer";
    case 121102:
        return "Stage Insert Position";
    case 121131:
        return "Laser/Lamp Intensity";
    case 268435456:
        return "Units";
    case vsi::VALUE:
        return "Value";
    case 175208:
        return "Snapshot Count";
    case 175209:
        return "Scanning Time (seconds)";
    case 120210:
        return "Device Configuration Position";
    case 120211:
        return "Device Configuration Index";
    case 124000:
        return "Aperture Max Mode";
    case vsi::FRAME_SIZE:
        return "Camera Maximum Frame Size";
    case vsi::HDRI_ON:
        return "Camera HDRI Enabled";
    case vsi::HDRI_FRAMES:
        return "Camera Images per HDRI image";
    case vsi::HDRI_EXPOSURE_RANGE:
        return "Camera HDRI Exposure Ratio";
    case vsi::HDRI_MAP_MODE:
        return "Camera HDRI Mapping Mode";
    case vsi::CUSTOM_GRAYSCALE:
        return "Camera Custom Grayscale Value";
    case vsi::SATURATION:
        return "Camera Saturation";
    case vsi::WB_PRESET_ID:
        return "Camera White Balance Preset ID";
    case vsi::WB_PRESET_NAME:
        return "Camera White Balance Preset Name";
    case vsi::WB_MODE:
        return "Camera White Balance Mode";
    case vsi::CCD_SENSITIVITY:
        return "Camera CCD Sensitivity";
    case vsi::ENHANCED_DYNAMIC_RANGE:
        return "Camera Enhanced Dynamic Range";
    case vsi::PIXEL_CLOCK:
        return "Camera Pixel Clock (MHz)";
    case vsi::COLORSPACE:
        return "Camera Colorspace";
    case vsi::COOLING_ON:
        return "Camera Cooling Enabled";
    case vsi::FAN_SPEED:
        return "Camera Cooling Fan Speed";
    case vsi::TEMPERATURE_TARGET:
        return "Camera Cooling Temperature Target";
    case vsi::GAIN_UNIT:
        return "Camera Gain Unit";
    case vsi::EM_GAIN:
        return "Camera EM Gain";
    case vsi::PHOTON_IMAGING_MODE:
        return "Camera Photon Imaging Mode";
    case vsi::FRAME_TRANSFER:
        return "Camera Frame Transfer Enabled";
    case vsi::ANDOR_SHIFT_SPEED:
        return "Camera iXon Shift Speed";
    case vsi::VCLOCK_AMPLITUDE:
        return "Camera Vertical Clock Amplitude";
    case vsi::SPURIOUS_NOISE_REMOVAL:
        return "Camera Spurious Noise Removal Enabled";
    case vsi::SIGNAL_OUTPUT:
        return "Camera Signal Output";
    case vsi::BASELINE_OFFSET_CLAMP:
        return "Camera Baseline Offset Clamp";
    case vsi::DP80_FRAME_CENTERING:
        return "Camera DP80 Frame Centering";
    case vsi::HOT_PIXEL_CORRECTION:
        return "Camera Hot Pixel Correction Enabled";
    case vsi::NOISE_REDUCTION:
        return "Camera Noise Reduction";
    case vsi::WIDER:
        return "Camera WiDER";
    case vsi::PHOTOBLEACHING:
        return "Camera Photobleaching Enabled";
    case vsi::PREAMP_GAIN_VALUE:
        return "Camera Preamp Gain";
    case vsi::WIDER_ENABLED:
        return "Camera WiDER Enabled";
    }
    return "";

}


VSISlide::~VSISlide()
{
}

int VSISlide::getNumScenes() const
{
    return (int)m_Scenes.size();
}

std::string VSISlide::getFilePath() const
{
    return m_filePath;
}

std::shared_ptr<CVScene> VSISlide::getScene(int index) const
{
    if(index>=getNumScenes()) {
        RAISE_RUNTIME_ERROR << "VSI driver: invalid m_scene index: " << index << " from " << getNumScenes() << " scenes";
    }
    return m_Scenes[index];
}

std::shared_ptr<CVScene> VSISlide::getAuxImage(const std::string& sceneName) const
{
    auto it = m_auxImages.find(sceneName);
    if (it == m_auxImages.end()) {
        RAISE_RUNTIME_ERROR << "The slide does non have auxiliary image " << sceneName;
    }
    return it->second;
}

std::string VSISlide::getStackType(const std::string& value)
{
    int stackType = std::stoi(value);
    switch (stackType) {
    case vsi::DEFAULT_IMAGE:
        return "Default image";
    case vsi::OVERVIEW_IMAGE:
        return "Overview image";
    case vsi::SAMPLE_MASK:
        return "Sample mask";
    case vsi::FOCUS_IMAGE:
        return "Focus image";
    case vsi::EFI_SHARPNESS_MAP:
        return "EFI sharpness map";
    case vsi::EFI_HEIGHT_MAP:
        return "EFI height map";
    case vsi::EFI_TEXTURE_MAP:
        return "EFI texture map";
    case vsi::EFI_STACK:
        return "EFI stack";
    case vsi::MACRO_IMAGE:
        return "Macro image";
    }
    return value;
}

std::string VSISlide::getDeviceSubtype(const std::string& value)
{
    int deviceType = std::stoi(value);
    switch (deviceType) {
    case 0:
        return "Camera";
    case 10000:
        return "Stage";
    case 20000:
        return "Objective revolver";
    case 20001:
        return "TV Adapter";
    case 20002:
        return "Filter Wheel";
    case 20003:
        return "Lamp";
    case 20004:
        return "Aperture Stop";
    case 20005:
        return "Shutter";
    case 20006:
        return "Objective";
    case 20007:
        return "Objective Changer";
    case 20008:
        return "TopLens";
    case 20009:
        return "Prism";
    case 20010:
        return "Zoom";
    case 20011:
        return "DSU";
    case 20012:
        return "ZDC";
    case 20050:
        return "Stage Insert";
    case 30000:
        return "Slide Loader";
    case 40000:
        return "Manual Control";
    case 40500:
        return "Microscope Frame";
    }
    return value;
}
