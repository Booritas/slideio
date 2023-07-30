// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "vsitools.hpp"
#include "vsislide.hpp"
#include "vsitags.hpp"

using namespace slideio;

DataType vsi::VSITools::toSlideioPixelType(uint32_t vsiPixelType)
{
    switch(vsiPixelType)
    {
      case vsi::PixelType::CHAR: return DataType::DT_Int8;
      case vsi::PixelType::UCHAR: return DataType::DT_Byte;
      case vsi::PixelType::SHORT: return DataType::DT_Int16;
	  case vsi::PixelType::USHORT: return DataType::DT_UInt16;
	  case vsi::PixelType::INT: return DataType::DT_Int32;
	  case vsi::PixelType::UINT: return DataType::DT_UInt32;
	  case vsi::PixelType::INT64: return DataType::DT_Int64;
	  case vsi::PixelType::UINT64: return DataType::DT_UInt64;
	  case vsi::PixelType::FLOAT: return DataType::DT_Float32;
	  case vsi::PixelType::DOUBLE: return DataType::DT_Float64;
    }
	RAISE_RUNTIME_ERROR << "VSI Driver: Unsupported pixel type: " << vsiPixelType;
}

Compression vsi::VSITools::toSlideioCompression(vsi::Compression format)
{
	switch(format) {
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

std::string vsi::VSITools::getVolumeName(int32_t tag)
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

std::string vsi::VSITools::getTagName(int32_t tag)
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

std::string vsi::VSITools::getStackTypeName(const std::string& value)
{
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

std::string vsi::VSITools::getDeviceSubtype(const std::string& value)
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

