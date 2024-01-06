// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "vsitools.hpp"
#include "vsislide.hpp"
#include "vsitags.hpp"

using namespace slideio;

DataType vsi::VSITools::toSlideioPixelType(uint32_t vsiPixelType) {
    switch (static_cast<vsi::ValueType>(vsiPixelType)) {
    case vsi::ValueType::CHAR: return DataType::DT_Int8;
    case vsi::ValueType::UCHAR: return DataType::DT_Byte;
    case vsi::ValueType::SHORT: return DataType::DT_Int16;
    case vsi::ValueType::USHORT: return DataType::DT_UInt16;
    case vsi::ValueType::INT: return DataType::DT_Int32;
    case vsi::ValueType::UINT: return DataType::DT_UInt32;
    case vsi::ValueType::INT64: return DataType::DT_Int64;
    case vsi::ValueType::UINT64: return DataType::DT_UInt64;
    case vsi::ValueType::FLOAT: return DataType::DT_Float32;
    case vsi::ValueType::DOUBLE: return DataType::DT_Float64;
    }
    RAISE_RUNTIME_ERROR << "VSI Driver: Unsupported pixel type: " << vsiPixelType;
}

Compression vsi::VSITools::toSlideioCompression(vsi::Compression format) {
    switch (format) {
    case vsi::Compression::RAW: return slideio::Compression::Uncompressed;
    case vsi::Compression::JPEG: return slideio::Compression::Jpeg;
    case vsi::Compression::JPEG_2000: return slideio::Compression::Jpeg2000;
    case vsi::Compression::JPEG_LOSSLESS: return slideio::Compression::JpegLossless;
    case vsi::Compression::PNG: return slideio::Compression::Png;
    case vsi::Compression::BMP: return slideio::Compression::BMP;
    }
    RAISE_RUNTIME_ERROR << "VSI Driver: Unsupported compression type: " << static_cast<uint32_t>(format);
}

vsi::StackType vsi::VSITools::intToStackType(int value) {
    switch (value) {
    case 0:
        return vsi::StackType::DEFAULT_IMAGE;
    case 1:
        return vsi::StackType::OVERVIEW_IMAGE;
    case 2:
        return vsi::StackType::SAMPLE_MASK;
    case 4:
        return vsi::StackType::FOCUS_IMAGE;
    case 8:
        return vsi::StackType::EFI_SHARPNESS_MAP;
    case 16:
        return vsi::StackType::EFI_HEIGHT_MAP;
    case 32:
        return vsi::StackType::EFI_TEXTURE_MAP;
    case 64:
        return vsi::StackType::EFI_STACK;
    case 256:
        return vsi::StackType::MACRO_IMAGE;
    default:
        return vsi::StackType::UNKNOWN;
    }
}

std::string vsi::VSITools::getVolumeName(int tag) {
    switch (tag) {
    case vsi::Tag::COLLECTION_VOLUME:
    case vsi::Tag::MULTIDIM_IMAGE_VOLUME:
    case vsi::Tag::IMAGE_FRAME_VOLUME:
    case vsi::Tag::DIMENSION_SIZE:
    case vsi::Tag::IMAGE_COLLECTION_PROPERTIES:
    case vsi::Tag::MULTIDIM_STACK_PROPERTIES:
    case vsi::Tag::FRAME_PROPERTIES:
    case vsi::Tag::DIMENSION_DESCRIPTION_VOLUME:
    case vsi::Tag::CHANNEL_PROPERTIES:
    case vsi::Tag::DISPLAY_MAPPING_VOLUME:
    case vsi::Tag::LAYER_INFO_PROPERTIES:
        return "";
    case vsi::Tag::MICROSCOPE:
        return "Microscope ";
    case vsi::Tag::CHANNEL_WAVELENGTH:
        return "Channel Wavelength ";
    case vsi::Tag::WORKING_DISTANCE:
        return "Objective Working Distance ";
    }
    return "";
}

bool vsi::VSITools::isTag(const boost::json::object& parentObject, int srcTag) {
    bool ret = false;
    if (parentObject.contains("tag")) {
        const int trgTag = static_cast<int>(parentObject.at("tag").as_int64());
        if (trgTag == srcTag) {
            ret = true;
        }
    }
    return ret;
}

std::string vsi::VSITools::getDimensionPropertyName(int tag) {
    switch (tag) {
    case Tag::Z_START:
        return "Z stack start";
    case Tag::Z_INCREMENT:
        return "Z stack increment";
    case Tag::Z_VALUE:
        return "Z value";
    case Tag::TIME_START:
        return "Time start";
    case Tag::TIME_INCREMENT:
        return "Time increment";
    case Tag::TIME_VALUE:
        return "Time value";
    case Tag::LAMBDA_START:
        return "Lambda start";
    case Tag::LAMBDA_INCREMENT:
        return "Lambda increment";
    case Tag::LAMBDA_VALUE:
        return "Lambda value";
    case Tag::DIMENSION_NAME:
        return "Dimension name";
    case Tag::DIMENSION_MEANING:
        return "Dimension description";
    case Tag::DIMENSION_START_ID:
        return "Dimension start ID";
    case Tag::DIMENSION_INCREMENT_ID:
        return "Dimension increment ID";
    case Tag::DIMENSION_VALUE_ID:
        return "Dimension value ID";
    }
    return "Unknown dimension property";
}

std::string vsi::VSITools::getStackPropertyName(int tag) {
    switch (tag) {
    case Tag::DISPLAY_LIMITS:
        return "Display limits";
    case Tag::STACK_DISPLAY_LUT:
        return "Display LUT";
    case Tag::GAMMA_CORRECTION:
        return "Gamma correction";
    case Tag::FRAME_ORIGIN:
        return "Frame origin";
    case Tag::FRAME_SCALE:
        return "Frame scale";
    case Tag::DISPLAY_COLOR:
        return "Display color";
    case Tag::CREATION_TIME:
        return "Creation time";
    case Tag::RWC_FRAME_ORIGIN:
        return "RWC frame origin";
    case Tag::RWC_FRAME_SCALE:
        return "RWC frame scale";
    case Tag::RWC_FRAME_UNIT:
        return "RWC frame unit";
    case vsi::Tag::STACK_NAME:
        return "Stack name";
    case Tag::CHANNEL_DIM:
        return "Channel dimension";
    case Tag::STACK_TYPE:
        return "Stack type";
    case Tag::LIVE_OVERFLOW:
        return "Live overflow";
    case Tag::CHANNEL_WAVELENGTH:
        return "Channel wavelength";
    case Tag::IS_TRANSMISSION:
        return "Transmission";
    case Tag::CONTRAST_BRIGHTNESS:
        return "Contrast brightness";
    case Tag::ACQUISITION_PROPERTIES:
        return "Acquisition properties";
    case Tag::GRADIENT_LUT:
        return "Gradient LUT";
    case Tag::Y_DIMENSION_UNIT:
        return "Y dimension unit";
    case Tag::LAYER_XY_FIXED:
        return "Layer XY fixed";
    case Tag::MICROSCOPE:
        return "Microscope";
    }
    return "Unknown stack property";
}

std::string vsi::VSITools::getTagName(const TagInfo& tagInfo, const boost::json::object& parentObject) {

    if (isTag(parentObject, Tag::PROPERTY_SET_VOLUME_FOR_DOCUMENT_PROPERTIES)) {
        switch (tagInfo.tag) {
        case Tag::DOCUMENT_NAME:
            return "Document name";
        case Tag::DOCUMENT_NOTE:
            return "Document note";
        case Tag::DOCUMENT_TIME:
            return "Document creation time";
        case Tag::DOCUMENT_AUTHOR:
            return "Document author";
        case Tag::DOCUMENT_COMPANY:
            return "Document company";
        case Tag::DOCUMENT_CREATOR_NAME:
            return "Document creator name";
        case Tag::DOCUMENT_CREATOR_MAJOR_VERSION:
            return "Document creator major version";
        case Tag::DOCUMENT_CREATOR_MINOR_VERSION:
            return "Document creator minor version";
        case Tag::DOCUMENT_CREATOR_SUB_VERSION:
            return "Document creator sub version";
        case Tag::DOCUMENT_CREATOR_BUILD_NUMBER:
            return "Document creator build number";
        case Tag::DOCUMENT_CREATOR_PACKAGE:
            return "Document creator package";
        case Tag::DOCUMENT_PRODUCT:
            return "Document product";
        case Tag::DOCUMENT_PRODUCT_NAME:
            return "Document product name";
        case Tag::DOCUMENT_PRODUCT_VERSION:
            return "Document product version";
        case Tag::DOCUMENT_TYPE_HINT:
            return "Document type hint";
        case Tag::DOCUMENT_THUMB:
            return "Document thumbnail";
        }
    }

    if (isTag(parentObject, Tag::COLLECTION_VOLUME)) {
        switch (tagInfo.tag) {
        case Tag::VERSION_NUMBER:
            return "Version number";
        case Tag::DEFAULT_SAMPLE_PIXEL_DATA_IFD:
            return "Default sample pixel data IFD";
        case Tag::MULTIDIM_IMAGE_VOLUME:
            return "Multidimensional image volume";
        case Tag::IMAGE_COLLECTION_PROPERTIES:
            return "Image collection properties";
        case Tag::DISPLAY_MAPPING_VOLUME:
            return "Display mapping volume";
        }
    }

    if (isTag(parentObject, Tag::MULTIDIM_STACK_PROPERTIES)) {
        return getStackPropertyName(tagInfo.tag);
    }

    if (isTag(parentObject, Tag::DIMENSION_DESCRIPTION_VOLUME)) {
        return getDimensionPropertyName(tagInfo.tag);
    }

    if(isTag(parentObject, Tag::MICROSCOPE)) {
        if(tagInfo.tag == Tag::MICROSCOPE_PROPERTIES) {
            return "Microscope properties";
        }
    }

    if(isTag(parentObject, Tag::MICROSCOPE_PROPERTIES)) {
        if(tagInfo.tag == Tag::OPTICAL_PROPERTIES) {
            return "Optical properties";
        }
    }

    if (isTag(parentObject, Tag::IMAGE_FRAME_VOLUME)) {
        switch (tagInfo.tag) {
        case Tag::DEFAULT_SAMPLE_PIXEL_DATA_IFD:
            return "Default sample IFD";
        case Tag::EXTERNAL_FILE_PROPERTIES:
            return "External file properties";
        }
    }

    if (tagInfo.extendedType == ExtendedType::PROPERTY_SET_VOLUME
        || tagInfo.extendedType == ExtendedType::NEW_MDIM_VOLUME_HEADER
        || tagInfo.extendedType == ExtendedType::NEW_VOLUME_HEADER) {
        switch (tagInfo.tag) {
        case vsi::Tag::DIMENSION_SIZE:
            return "Dimension size";
        case vsi::Tag::MULTIDIM_STACK_PROPERTIES:
            return "Multidimensional stack properties";
        case vsi::Tag::FRAME_PROPERTIES:
            return "Frame properties";
        case vsi::Tag::DIMENSION_DESCRIPTION_VOLUME:
            return std::string("Volume for dimension ") +
                std::to_string(tagInfo.secondTag) + std::string(" description");
        case vsi::Tag::CHANNEL_PROPERTIES:
            return "Channel properties";
        case vsi::Tag::LAYER_INFO_PROPERTIES:
            return "Layer info properties";
        case vsi::Tag::MICROSCOPE:
            return "Microscope ";
        case vsi::Tag::CHANNEL_WAVELENGTH:
            return "Channel Wavelength ";
        case vsi::Tag::WORKING_DISTANCE:
            return "Objective Working Distance ";
        case vsi::Tag::CHANNEL_INFO_PROPERTIES:
            return "Channel info properties";
        }
    }


    switch (tagInfo.tag) {
    case vsi::Tag::COLLECTION_VOLUME:
        return "Collection volume";
    case vsi::Tag::PROPERTY_SET_VOLUME_FOR_DOCUMENT_PROPERTIES:
        return "Document properties";
    case vsi::Tag::Y_PLANE_DIMENSION_UNIT:
        return "Image plane rectangle unit (Y dimension)";
    case vsi::Tag::Y_DIMENSION_UNIT:
        return "Y dimension unit";
    case vsi::Tag::CHANNEL_OVERFLOW:
        return "Channel under/overflow";
    case vsi::Tag::SLIDE_SPECIMEN:
        return "Specimen";
    case vsi::Tag::SLIDE_TISSUE:
        return "Tissue";
    case vsi::Tag::SLIDE_PREPARATION:
        return "Preparation";
    case vsi::Tag::SLIDE_STAINING:
        return "Staining";
    case vsi::Tag::SLIDE_INFO:
        return "Slide Info";
    case vsi::Tag::SLIDE_NAME:
        return "Slide Name";
    case vsi::Tag::EXPOSURE_TIME:
        return "Exposure time (microseconds)";
    case vsi::Tag::CAMERA_GAIN:
        return "Camera gain";
    case vsi::Tag::CAMERA_OFFSET:
        return "Camera offset";
    case vsi::Tag::CAMERA_GAMMA:
        return "Gamma";
    case vsi::Tag::SHARPNESS:
        return "Sharpness";
    case vsi::Tag::RED_GAIN:
        return "Red channel gain";
    case vsi::Tag::GREEN_GAIN:
        return "Green channel gain";
    case vsi::Tag::BLUE_GAIN:
        return "Blue channel gain";
    case vsi::Tag::RED_OFFSET:
        return "Red channel offset";
    case vsi::Tag::GREEN_OFFSET:
        return "Green channel offset";
    case vsi::Tag::BLUE_OFFSET:
        return "Blue channel offset";
    case vsi::Tag::SHADING_SUB:
        return "Shading sub";
    case vsi::Tag::SHADING_MUL:
        return "Shading mul";
    case vsi::Tag::X_BINNING:
        return "Binning (X)";
    case vsi::Tag::Y_BINNING:
        return "Binning (Y)";
    case vsi::Tag::CLIPPING:
        return "Clipping";
    case vsi::Tag::MIRROR_H:
        return "Mirror (horizontal)";
    case vsi::Tag::MIRROR_V:
        return "Mirror (vertical)";
    case vsi::Tag::CLIPPING_STATE:
        return "Clipping state";
    case vsi::Tag::ICC_ENABLED:
        return "ICC enabled";
    case vsi::Tag::BRIGHTNESS:
        return "Brightness";
    case vsi::Tag::CONTRAST:
        return "Contrast";
    case vsi::Tag::CONTRAST_TARGET:
        return "Contrast reference";
    case vsi::Tag::ACCUMULATION:
        return "Camera accumulation";
    case vsi::Tag::AVERAGING:
        return "Camera averaging";
    case vsi::Tag::ISO_SENSITIVITY:
        return "ISO sensitivity";
    case vsi::Tag::ACCUMULATION_MODE:
        return "Camera accumulation mode";
    case vsi::Tag::AUTOEXPOSURE:
        return "Autoexposure enabled";
    case vsi::Tag::EXPOSURE_METERING_MODE:
        return "Autoexposure metering mode";
    case vsi::Tag::Z_START:
        return "Z stack start";
    case vsi::Tag::Z_INCREMENT:
        return "Z stack increment";
    case vsi::Tag::Z_VALUE:
        return "Z position";
    case vsi::Tag::TIME_START:
        return "Timelapse start";
    case vsi::Tag::TIME_INCREMENT:
        if (tagInfo.fieldType == 0x1800000A) {
            return "TIFF IFD of the default sample";
        }
        return "Timelapse increment";
    case vsi::Tag::TIME_VALUE:
        return "Timestamp";
    case vsi::Tag::LAMBDA_START:
        return "Lambda start";
    case vsi::Tag::LAMBDA_INCREMENT:
        return "Lambda increment";
    case vsi::Tag::LAMBDA_VALUE:
        return "Lambda value";
    case vsi::Tag::DIMENSION_NAME:
        return "Dimension name";
    case vsi::Tag::DIMENSION_MEANING:
        return "Dimension description";
    case vsi::Tag::DIMENSION_START_ID:
        return "Dimension start ID";
    case vsi::Tag::DIMENSION_INCREMENT_ID:
        return "Dimension increment ID";
    case vsi::Tag::DIMENSION_VALUE_ID:
        return "Dimension value ID";
    case vsi::Tag::IMAGE_BOUNDARY:
        return "Image size";
    case vsi::Tag::TILE_SYSTEM:
        return "Tile system";
    case vsi::Tag::HAS_EXTERNAL_FILE:
        return "External file present";
    case vsi::Tag::EXTERNAL_DATA_VOLUME:
        return "External file volume";
    case vsi::Tag::TILE_ORIGIN:
        return "Origin of tile coordinate system";
    case vsi::Tag::DISPLAY_LIMITS:
        if (tagInfo.fieldType == 8195) {
            return "Multidimensional Index";
        }
        return "Display limits";
    case vsi::Tag::STACK_DISPLAY_LUT:
        return "Stack display LUT";
    case vsi::Tag::GAMMA_CORRECTION:
        return "Gamma correction";
    case vsi::Tag::FRAME_ORIGIN:
        return "Frame origin (plane coordinates)";
    case vsi::Tag::FRAME_SCALE:
        return "Frame scale (plane coordinates)";
    case vsi::Tag::DISPLAY_COLOR:
        return "Display color";
    case vsi::Tag::CREATION_TIME:
        return "Creation time (UTC)";
    case vsi::Tag::RWC_FRAME_ORIGIN:
        return "Origin";
    case vsi::Tag::RWC_FRAME_SCALE:
        return "Calibration";
    case vsi::Tag::RWC_FRAME_UNIT:
        return "Calibration units";
    case vsi::Tag::STACK_NAME:
        return "Sample flags";
    case vsi::Tag::CHANNEL_DIM:
        return "Channel dimension";
    case vsi::Tag::STACK_TYPE:
        return "Image Type";
    case vsi::Tag::LIVE_OVERFLOW:
        return "Live overflow";
    case vsi::Tag::IS_TRANSMISSION:
        return "IS transmission mask";
    case vsi::Tag::CONTRAST_BRIGHTNESS:
        return "Contrast and brightness";
    case vsi::Tag::ACQUISITION_PROPERTIES:
        return "Acquisition properties";
    case vsi::Tag::GRADIENT_LUT:
        return "Gradient LUT";
    case vsi::Tag::DISPLAY_PROCESSOR_TYPE:
        return "Display processor type";
    case vsi::Tag::RENDER_OPERATION_ID:
        return "Render operation ID";
    case vsi::Tag::DISPLAY_STACK_ID:
        return "Displayed stack ID";
    case vsi::Tag::TRANSPARENCY_ID:
        return "Transparency ID";
    case vsi::Tag::THIRD_ID:
        return "Display third ID";
    case vsi::Tag::DISPLAY_VISIBLE:
        return "Display visible";
    case vsi::Tag::TRANSPARENCY_VALUE:
        return "Transparency value";
    case vsi::Tag::DISPLAY_LUT:
        return "Display LUT";
    case vsi::Tag::DISPLAY_STACK_INDEX:
        return "Display stack index";
    case vsi::Tag::CHANNEL_TRANSPARENCY_VALUE:
        return "Channel transparency value";
    case vsi::Tag::CHANNEL_VISIBLE:
        return "Channel visible";
    case vsi::Tag::SELECTED_CHANNELS:
        return "List of selected channels";
    case vsi::Tag::DISPLAY_GAMMA_CORRECTION:
        return "Display gamma correction";
    case vsi::Tag::CHANNEL_GAMMA_CORRECTION:
        return "Channel gamma correction";
    case vsi::Tag::DISPLAY_CONTRAST_BRIGHTNESS:
        return "Display contrast and brightness";
    case vsi::Tag::CHANNEL_CONTRAST_BRIGHTNESS:
        return "Channel contrast and brightness";
    case vsi::Tag::ACTIVE_STACK_DIMENSION:
        return "Active stack dimension";
    case vsi::Tag::SELECTED_FRAMES:
        return "Selected frames";
    case vsi::Tag::DISPLAYED_LUT_ID:
        return "Displayed LUT ID";
    case vsi::Tag::HIDDEN_LAYER:
        return "Hidden layer";
    case vsi::Tag::LAYER_XY_FIXED:
        return "Layer fixed in XY";
    case vsi::Tag::ACTIVE_LAYER_VECTOR:
        return "Active layer vector";
    case vsi::Tag::ACTIVE_LAYER_INDEX_VECTOR:
        return "Active layer index vector";
    case vsi::Tag::CHAINED_LAYERS:
        return "Chained layers";
    case vsi::Tag::LAYER_SELECTION:
        return "Layer selection";
    case vsi::Tag::LAYER_SELECTION_INDEX:
        return "Layer selection index";
    case vsi::Tag::CANVAS_COLOR_1:
        return "Canvas background color 1";
    case vsi::Tag::CANVAS_COLOR_2:
        return "Canvas background color 2";
    case vsi::Tag::ORIGINAL_FRAME_RATE:
        return "Original frame rate (ms)";
    case vsi::Tag::USE_ORIGINAL_FRAME_RATE:
        return "Use original frame rate";
    case vsi::Tag::ACTIVE_CHANNEL:
        return "Active channel";
    case vsi::Tag::PLANE_UNIT:
        return "Plane unit";
    case vsi::Tag::PLANE_ORIGIN_RWC:
        return "Origin";
    case vsi::Tag::PLANE_SCALE_RWC:
        return "Physical pixel size";
    case vsi::Tag::MAGNIFICATION:
        return "Original magnification";
    case vsi::Tag::COARSE_PYRAMID_LEVEL:
        return "Coarse pyramid level";
    case vsi::Tag::EXTRA_SAMPLES:
        return "Extra samples";
    case vsi::Tag::DEFAULT_BACKGROUND_COLOR:
        return "Default background color";
    case vsi::Tag::VERSION_NUMBER:
        return "Version number";
    case vsi::Tag::CHANNEL_NAME:
        return "Channel name";
    case vsi::Tag::OBJECTIVE_MAG:
        return "Magnification";
    case vsi::Tag::NUMERICAL_APERTURE:
        return "Numerical Aperture";
    case vsi::Tag::WORKING_DISTANCE:
        return "Objective Working Distance";
    case vsi::Tag::OBJECTIVE_NAME:
        return "Objective Name";
    case vsi::Tag::OBJECTIVE_TYPE:
        return "Objective Type";
    case vsi::Tag::OBJECTIVE_DESCRIPTION: //120065:
        return "Objective Description";
    case vsi::Tag::OBJECTIVE_SUBTYPE: //120066:
        return "Objective Subtype";
    case vsi::Tag::BRIGHTNESS_CORRECTION: //120069:
        return "Brightness Correction";
    case vsi::Tag::OBJECTIVE_LENS: //120070:
        return "Objective Lens";
    case vsi::Tag::OBJECTIVE_X_SHIFT: //120075:
        return "Objective X Shift";
    case vsi::Tag::OBJECTIVE_Y_SHIFT: //120076:
        return "Objective Y Shift";
    case vsi::Tag::OBJECTIVE_Z_SHIFT: //120077:
        return "Objective Z Shift";
    case vsi::Tag::OBJECTIVE_GEAR_SETTING: //120078:
        return "Objective Gear Setting";
    case vsi::Tag::SLIDE_BAR_CODE: // 120635
        return "Slide Bar Code";
    case vsi::Tag::TRAY_NUMBER: // 120638
        return "Tray No.";
    case vsi::Tag::SLIDE_NUMBER: // 120637
        return "Slide No.";
    case vsi::Tag::PRODUCT_NAME: // 34
        return "Product Name";
    case vsi::Tag::PRODUCT_VERSION: // 35
        return "Product Version";
    case vsi::Tag::DEVICE_NAME:
        return "Device Name";
    case vsi::Tag::BIT_DEPTH:
        return "Camera Actual Bit Depth";
    case vsi::Tag::DEVICE_POSITION: // 120001
        return "Device Position";
    case vsi::Tag::TV_ADAPTER_MAGNIFICATION: // 120050
        return "TV Adapter Magnification";
    case vsi::Tag::OBJECTIVE_REFRACTIVE_INDEX:
        return "Objective Refractive Index";
    case vsi::Tag::DEVICE_TYPE: // 120117
        return "Device Type";
    case vsi::Tag::DEVICE_ID:
        return "Device Unit ID";
    case vsi::Tag::DEVICE_SUBTYPE:
        return "Device Subtype";
    case vsi::Tag::DEVICE_MODEL: //120132:
        return "Device Model";
    case vsi::Tag::DEVICE_MANUFACTURER:
        return "Device Manufacturer";
    case vsi::Tag::STAGE_INSERT_POSITION: // 121102
        return "Stage Insert Position";
    case vsi::Tag::LASER_LAMP_INTENSITY: // 121131
        return "Laser/Lamp Intensity";
    case vsi::Tag::UNITS: // 268435456
        return "Units";
    case vsi::Tag::VALUE: // 268435458
        return "Value";
    case vsi::Tag::SNAPSHOT_COUNT: // 175208
        return "Snapshot Count";
    case vsi::Tag::SCANNING_TIME: // 175209
        return "Scanning Time (seconds)";
    case vsi::Tag::DEVICE_CONFIGURATION_POSITION: // 120210
        return "Device Configuration Position";
    case vsi::Tag::DEVICE_CONFIGURATION_INDEX: // 120211
        return "Device Configuration Index";
    case vsi::Tag::APERTURE_MAX_MODE: // 124000
        return "Aperture Max Mode";
    case vsi::Tag::FRAME_SIZE:
        return "Camera Maximum Frame Size";
    case vsi::Tag::HDRI_ON:
        return "Camera HDRI Enabled";
    case vsi::Tag::HDRI_FRAMES:
        return "Camera Images per HDRI image";
    case vsi::Tag::HDRI_EXPOSURE_RANGE:
        return "Camera HDRI Exposure Ratio";
    case vsi::Tag::HDRI_MAP_MODE:
        return "Camera HDRI Mapping Mode";
    case vsi::Tag::CUSTOM_GRAYSCALE:
        return "Camera Custom Grayscale Value";
    case vsi::Tag::SATURATION:
        return "Camera Saturation";
    case vsi::Tag::WB_PRESET_ID:
        return "Camera White Balance Preset ID";
    case vsi::Tag::WB_PRESET_NAME:
        return "Camera White Balance Preset Name";
    case vsi::Tag::WB_MODE:
        return "Camera White Balance Mode";
    case vsi::Tag::CCD_SENSITIVITY:
        return "Camera CCD Sensitivity";
    case vsi::Tag::ENHANCED_DYNAMIC_RANGE:
        return "Camera Enhanced Dynamic Range";
    case vsi::Tag::PIXEL_CLOCK:
        return "Camera Pixel Clock (MHz)";
    case vsi::Tag::COLORSPACE:
        return "Camera Colorspace";
    case vsi::Tag::COOLING_ON:
        return "Camera Cooling Enabled";
    case vsi::Tag::FAN_SPEED:
        return "Camera Cooling Fan Speed";
    case vsi::Tag::TEMPERATURE_TARGET:
        return "Camera Cooling Temperature Target";
    case vsi::Tag::GAIN_UNIT:
        return "Camera Gain Unit";
    case vsi::Tag::EM_GAIN:
        return "Camera EM Gain";
    case vsi::Tag::PHOTON_IMAGING_MODE:
        return "Camera Photon Imaging Mode";
    case vsi::Tag::FRAME_TRANSFER:
        return "Camera Frame Transfer Enabled";
    case vsi::Tag::ANDOR_SHIFT_SPEED:
        return "Camera iXon Shift Speed";
    case vsi::Tag::VCLOCK_AMPLITUDE:
        return "Camera Vertical Clock Amplitude";
    case vsi::Tag::SPURIOUS_NOISE_REMOVAL:
        return "Camera Spurious Noise Removal Enabled";
    case vsi::Tag::SIGNAL_OUTPUT:
        return "Camera Signal Output";
    case vsi::Tag::BASELINE_OFFSET_CLAMP:
        return "Camera Baseline Offset Clamp";
    case vsi::Tag::DP80_FRAME_CENTERING:
        return "Camera DP80 Frame Centering";
    case vsi::Tag::HOT_PIXEL_CORRECTION:
        return "Camera Hot Pixel Correction Enabled";
    case vsi::Tag::NOISE_REDUCTION:
        return "Camera Noise Reduction";
    case vsi::Tag::WIDER:
        return "Camera WiDER";
    case vsi::Tag::PHOTOBLEACHING:
        return "Camera Photobleaching Enabled";
    case vsi::Tag::PREAMP_GAIN_VALUE:
        return "Camera Preamp Gain";
    case vsi::Tag::WIDER_ENABLED:
        return "Camera WiDER Enabled";
    case vsi::Tag::EXTRA_SAMPLES_PROPERTIES:
        return "Extra samples properties";
    }
    return "Unknown tag";
}

bool vsi::VSITools::isArray(const TagInfo& tagInfo) {
    if (tagInfo.tag == Tag::MULTIDIM_IMAGE_VOLUME) {
        return true;
    }
    else if (tagInfo.tag == 0 || tagInfo.tag == 1 || tagInfo.tag == Tag::LAYER_INFO_PROPERTIES) {
        return true;
    }
    return false;
}

std::string vsi::VSITools::getStackTypeName(const std::string& value) {
    auto stackType = static_cast<StackType>(std::stoi(value));
    switch (stackType) {
    case StackType::DEFAULT_IMAGE:
        return "Default image";
    case StackType::OVERVIEW_IMAGE:
        return "Overview image";
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
    }
    return value;
}

std::string vsi::VSITools::getDeviceSubtype(const std::string& value) {
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

std::string vsi::VSITools::extractTagValue(vsi::VSIStream& vsi, const vsi::TagInfo& tagInfo) {
    std::string value;
    switch (tagInfo.valueType) {
    case vsi::ValueType::CHAR:
    case vsi::ValueType::UCHAR:
        value = std::to_string(vsi.readValue<uint8_t>());
        break;
    case vsi::ValueType::SHORT:
    case vsi::ValueType::USHORT:
        value = std::to_string(vsi.readValue<uint16_t>());
        break;
    case vsi::ValueType::INT:
    case vsi::ValueType::UINT:
    case vsi::ValueType::DWORD:
    case vsi::ValueType::FIELD_TYPE:
    case vsi::ValueType::MEM_MODEL:
    case vsi::ValueType::COLOR_SPACE:
        value = std::to_string(vsi.readValue<uint32_t>());
        break;
    case vsi::ValueType::INT64:
    case vsi::ValueType::UINT64:
    case vsi::ValueType::TIMESTAMP:
        value = std::to_string(vsi.readValue<uint64_t>());
        break;
    case vsi::ValueType::FLOAT:
        value = std::to_string(vsi.readValue<float>());
        break;
    case vsi::ValueType::DOUBLE:
    case vsi::ValueType::DATE:
        value = std::to_string(vsi.readValue<double>());
        break;
    case vsi::ValueType::BOOL:
        value = std::to_string(vsi.readValue<bool>());
        break;
    case vsi::ValueType::TCHAR:
    case vsi::ValueType::UNICODE_TCHAR:
        value = vsi.readString(tagInfo.dataSize);
        break;
    case vsi::ValueType::VECTOR_INT_2:
    case vsi::ValueType::TUPLE_INT:
    case vsi::ValueType::ARRAY_INT_2:
    case vsi::ValueType::VECTOR_INT_3:
    case vsi::ValueType::ARRAY_INT_3:
    case vsi::ValueType::VECTOR_INT_4:
    case vsi::ValueType::RECT_INT:
    case vsi::ValueType::ARRAY_INT_4:
    case vsi::ValueType::ARRAY_INT_5:
    case vsi::ValueType::DIM_INDEX_1:
    case vsi::ValueType::DIM_INDEX_2:
    case vsi::ValueType::VOLUME_INDEX:
    case vsi::ValueType::PIXEL_INFO_TYPE: {
        uint32_t nIntValues = tagInfo.dataSize / 4;
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
    }
    break;
    case vsi::ValueType::DOUBLE2:
    case vsi::ValueType::VECTOR_DOUBLE_2:
    case vsi::ValueType::TUPLE_DOUBLE:
    case vsi::ValueType::ARRAY_DOUBLE_2:
    case vsi::ValueType::VECTOR_DOUBLE_3:
    case vsi::ValueType::ARRAY_DOUBLE_3:
    case vsi::ValueType::VECTOR_DOUBLE_4:
    case vsi::ValueType::RECT_DOUBLE:
    case vsi::ValueType::MATRIX_DOUBLE_2_2:
    case vsi::ValueType::MATRIX_DOUBLE_3_3:
    case vsi::ValueType::MATRIX_DOUBLE_4_4: {
        int nDoubleValues = tagInfo.dataSize / sizeof(double);
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
    }
    break;
    case vsi::ValueType::RGB: {
        int red = vsi.readValue<uint8_t>();
        int green = vsi.readValue<uint8_t>();
        int blue = vsi.readValue<uint8_t>();
        value = "red = " + std::to_string(red)
            + ", green = " + std::to_string(green)
            + ", blue = " + std::to_string(blue);
    }
    break;
    case vsi::ValueType::BGR: {
        int blue = vsi.readValue<uint8_t>();
        int green = vsi.readValue<uint8_t>();
        int red = vsi.readValue<uint8_t>();
        value = "red = " + std::to_string(red)
            + ", green = " + std::to_string(green)
            + ", blue = " + std::to_string(blue);
    }
    break;
    }
    return value;
}

boost::json::value vsi::VSITools::findMetadataObject(boost::json::object& parent, const std::vector<int>& path) {
    boost::json::value current = parent;
    boost::json::value empty(nullptr);

    for (const int tag : path) {
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
