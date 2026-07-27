// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/zvi/zvitags.hpp"

namespace slideio
{

    const char* getZviTagName(int32_t id)
    {
        switch (static_cast<ZVITAG>(id))
        {
        case ZVITAG::ZVITAG_COMPRESSION:
            return "Compression";
        case ZVITAG::ZVITAG_DATE_MAPPING_TABLE:
            return "DateMappingTable";
        case ZVITAG::ZVITAG_BLACK_VALUE:
            return "Black Value";
        case ZVITAG::ZVITAG_WHITE_VALUE:
            return "White value";
        case ZVITAG::ZVITAG_IMAGE_DATA_MAPPING_AUTO_RANGE:
            return "ImageDataMappingAutoRange";
        case ZVITAG::ZVITAG_IMAGE_THUMBNAIL:
            return "Image Thumbnail";
        case ZVITAG::ZVITAG_GAMMA_VALUE:
            return "Gamma Value";
        case ZVITAG::ZVITAG_IMAGE_OVER_EXPOSURE:
            return "ImageOverExposure";
        case ZVITAG::ZVITAG_IMAGE_RELATIVE_TIME1:
            return "ImageRelativeTime1";
        case ZVITAG::ZVITAG_IMAGE_RELATIVE_TIME2:
            return "ImageRelativeTime2";
        case ZVITAG::ZVITAG_IMAGE_RELATIVE_TIME3:
            return "ImageRelativeTime3";
        case ZVITAG::ZVITAG_IMAGE_RELATIVE_TIME4:
            return "ImageRelativeTime4";
        case ZVITAG::ZVITAG_IMAGE_RELATIVE_TIME:
            return "ImageRelativeTime";
        case ZVITAG::ZVITAG_IMAGE_BASE_TIME_FIRST:
            return "ImageBaseTimeFirst";
        case ZVITAG::ZVITAG_IMAGE_BASE_TIME2:
            return "ImageBaseTime2";
        case ZVITAG::ZVITAG_IMAGE_BASE_TIME3:
            return "ImageBaseTime3";
        case ZVITAG::ZVITAG_IMAGE_BASE_TIME4:
            return "ImageBaseTime4";
        case ZVITAG::ZVITAG_IMAGE_WIDTH:
            return "Image Width (Pixel)";
        case ZVITAG::ZVITAG_IMAGE_HEIGHT:
            return "Image Height (Pixel)";
        case ZVITAG::ZVITAG_IMAGE_COUNT:
            return "ImageCountRaw";
        case ZVITAG::ZVITAG_IMAGE_PIXEL_TYPE:
            return "Pixel Type";
        case ZVITAG::ZVITAG_NUMBER_RAW_IMAGES:
            return "Number Raw Images";
        case ZVITAG::ZVITAG_IMAGE_SIZE:
            return "Image Size";
        case ZVITAG::ZVITAG_COMPRESSION_FACTOR_FOR_SAVE:
            return "CompressionFactorForSave";
        case ZVITAG::ZVITAG_DOCUMENT_SAVE_FLAGS:
            return "DocumentSaveFlags";
        case ZVITAG::ZVITAG_ACQUISITION_PAUSE_ANNOTATION:
            return "Acquisition pause annotation";
        case ZVITAG::ZVITAG_DOCUMENT_SUBTYPE:
            return "Document Subtype";
        case ZVITAG::ZVITAG_ACQUISITION_BIT_DEPTH:
            return "Acquisition Bit Depth";
        case ZVITAG::ZVITAG_ZSTACK_SINGLE_REPRESENTATIVE:
            return "Z-Stack single representative";
        case ZVITAG::ZVITAG_SCALE_X:
            return "Scale Factor For X";
        case ZVITAG::ZVITAG_SCALE_UNIT_X:
            return "Scale Unit for X";
        case ZVITAG::ZVITAG_SCALE_WIDTH:
            return "Scale Width";
        case ZVITAG::ZVITAG_SCALE_Y:
            return "Scale Factor For Y";
        case ZVITAG::ZVITAG_SCALE_UNIT_Y:
            return "Scale Unit for Y";
        case ZVITAG::ZVITAG_SCALE_HEIGHT:
            return "Scale Height";
        case ZVITAG::ZVITAG_SCALE_Z:
            return "Scale Factor For Z";
        case ZVITAG::ZVITAG_SCALE_UNIT_Z:
            return "Scale Unit for Z";
        case ZVITAG::ZVITAG_SCALE_DEPTH:
            return "Scale Depth";
        case ZVITAG::ZVITAG_SCALING_PARENT:
            return "Scaling Parent";
        case ZVITAG::ZVITAG_DATE:
            return "Date";
        case ZVITAG::ZVITAG_CODE:
            return "Code";
        case ZVITAG::ZVITAG_SOURCE:
            return "Source";
        case ZVITAG::ZVITAG_MESSAGE:
            return "Message";
        case ZVITAG::ZVITAG_CAMERA_IMAGE_ACQUISITION_TIME:
            return "CameraImageAcquisitionTime";
        case ZVITAG::ZVITAG_8BIT_ACQUISITION:
            return "8-bit Acquisition";
        case ZVITAG::ZVITAG_CAMERA_BIT_DEPTH:
            return "Camera Bit Depth";
        case ZVITAG::ZVITAG_MONO_REFERENCE_LOW:
            return "MonoReferenceLow";
        case ZVITAG::ZVITAG_MONO_REFERENCE_HIGH:
            return "MonoReferenceHigh";
        case ZVITAG::ZVITAG_RED_REFERENCE_LOW:
            return "RedReferenceLow";
        case ZVITAG::ZVITAG_RED_REFERENCE_HIGH:
            return "RedReferenceHigh";
        case ZVITAG::ZVITAG_GREEN_REFERENCE_LOW:
            return "GreenReferenceLow";
        case ZVITAG::ZVITAG_GREEN_REFERENCE_HIGH:
            return "GreenReferenceHigh";
        case ZVITAG::ZVITAG_BLUE_REFERENCE_LOW:
            return "BlueReferenceLow";
        case ZVITAG::ZVITAG_BLUE_REFERENCE_HIGH:
            return "BlueReferenceHigh";
        case ZVITAG::ZVITAG_FRAMEGRABBER_NAME:
            return "Framegrabber Name";
        case ZVITAG::ZVITAG_CAMERA:
            return "Camera";
        case ZVITAG::ZVITAG_CAMERA_TRIGGER_SIGNAL_TYPE:
            return "CameraTriggerSignalType";
        case ZVITAG::ZVITAG_CAMERA_TRIGGER_ENABLE:
            return "CameraTriggerEnable";
        case ZVITAG::ZVITAG_GRABBER_TIMEOUT:
            return "GrabberTimeout";
        case ZVITAG::ZVITAG_MULTICHANNEL_ENABLED:
            return "MultiChannelEnabled";
        case ZVITAG::ZVITAG_MULTICHANNEL_COLOUR:
            return "Multichannel Colour";
        case ZVITAG::ZVITAG_MULTICHANNEL_WEIGHT:
            return "Multichannel Weight";
        case ZVITAG::ZVITAG_CHANNEL_NAME:
            return "Channel Name";
        case ZVITAG::ZVITAG_DOCUMENT_INFORMATION_GROUP:
            return "DocumentInformationGroup";
        case ZVITAG::ZVITAG_TITLE:
            return "Title";
        case ZVITAG::ZVITAG_AUTHOR:
            return "Author";
        case ZVITAG::ZVITAG_KEYWORDS:
            return "Keywords";
        case ZVITAG::ZVITAG_COMMENTS:
            return "Comments";
        case ZVITAG::ZVITAG_SAMPLE_ID:
            return "Sample ID";
        case ZVITAG::ZVITAG_SUBJECT:
            return "Subject";
        case ZVITAG::ZVITAG_REVISION_NUMBER:
            return "RevisionNumber";
        case ZVITAG::ZVITAG_SAVE_FOLDER:
            return "Save Folder";
        case ZVITAG::ZVITAG_FILE_LINK:
            return "FileLink";
        case ZVITAG::ZVITAG_DOCUMENT_TYPE:
            return "Document Type";
        case ZVITAG::ZVITAG_STORAGE_MEDIA:
            return "Storage Media";
        case ZVITAG::ZVITAG_FILE_ID:
            return "File ID";
        case ZVITAG::ZVITAG_REFERENCE:
            return "Reference";
        case ZVITAG::ZVITAG_FILE_DATE:
            return "File Date";
        case ZVITAG::ZVITAG_FILE_SIZE:
            return "File Size";
        case ZVITAG::ZVITAG_FILE_NAME:
            return "Filename";
        case ZVITAG::ZVITAG_FILE_ATTRIBUTES:
            return "FileAttributes";
        case ZVITAG::ZVITAG_PROJECT_GROUP:
            return "ProjectGroup";
        case ZVITAG::ZVITAG_ACQUISITION_DATE:
            return "Acquisition Date";
        case ZVITAG::ZVITAG_LAST_MODIFIED_BY:
            return "Last modified by";
        case ZVITAG::ZVITAG_USER_COMPANY:
            return "User Company";
        case ZVITAG::ZVITAG_USER_COMPANY_LOGO:
            return "User Company Logo";
        case ZVITAG::ZVITAG_USER_IMAGE:
            return "Image";
        case ZVITAG::ZVITAG_USER_ID:
            return "User ID";
        case ZVITAG::ZVITAG_USER_NAME:
            return "User Name";
        case ZVITAG::ZVITAG_USER_CITY:
            return "User City";
        case ZVITAG::ZVITAG_USER_ADDRESS:
            return "User Address";
        case ZVITAG::ZVITAG_USER_COUNTRY:
            return "User Country";
        case ZVITAG::ZVITAG_USER_PHONE:
            return "User Phone";
        case ZVITAG::ZVITAG_USER_FAX:
            return "User Fax";
        case ZVITAG::ZVITAG_OBJECTIVE_NAME:
            return "Objective Name";
        case ZVITAG::ZVITAG_OPTOVAR:
            return "Optovar";
        case ZVITAG::ZVITAG_REFLECTOR:
            return "Reflector";
        case ZVITAG::ZVITAG_CONDENSER_CONTRAST:
            return "Condenser Contrast";
        case ZVITAG::ZVITAG_TRANSMITTED_LIGHT_FILTER_1:
            return "Transmitted Light Filter 1";
        case ZVITAG::ZVITAG_TRANSMITTED_LIGHT_FILTER_2:
            return "Transmitted Light Filter 2";
        case ZVITAG::ZVITAG_REFLECTED_LIGHT_SHUTTER:
            return "Reflected Light Shutter";
        case ZVITAG::ZVITAG_CONDENSER_FRONT_LENS:
            return "Condenser Front Lens";
        case ZVITAG::ZVITAG_EXCITATION_FILTER_NAME:
            return "Excitation Filer Name";
        case ZVITAG::ZVITAG_TRANSMITTED_LIGHT_FIELDSTOP_APERTURE:
            return "Transmitted Light Fieldstop Aperture";
        case ZVITAG::ZVITAG_REFLECTED_LIGHT_APERTURE:
            return "Reflected Light Aperture";
        case ZVITAG::ZVITAG_CONDENSER_NA:
            return "Condenser N.A.";
        case ZVITAG::ZVITAG_LIGHT_PATH:
            return "Light Path";
        case ZVITAG::ZVITAG_HALOGEN_LAMP_ON:
            return "HalogenLampOn";
        case ZVITAG::ZVITAG_HALOGEN_LAMP_MODE:
            return "Halogen Lamp Mode";
        case ZVITAG::ZVITAG_HALOGEN_LAMP_VOLTAGE:
            return "Halogen Lamp Voltage";
        case ZVITAG::ZVITAG_FLUORESCENCE_LAMP_LEVEL:
            return "Fluorescence Lamp Level";
        case ZVITAG::ZVITAG_FLUORESCENCE_LAMP_INTENSITY:
            return "Fluorescence Lamp Intensity";
        case ZVITAG::ZVITAG_LIGHT_MANAGER_ENABLED:
            return "Light Manager is Enabled";
        case ZVITAG::ZVITAG_FOCUS_POSITION:
            return "Focus Position";
        case ZVITAG::ZVITAG_STAGE_POSITION_X:
            return "Stage Position X";
        case ZVITAG::ZVITAG_STAGE_POSITION_Y:
            return "Stage Position Y";
        case ZVITAG::ZVITAG_MICROSCOPE_NAME:
            return "Microscope Name";
        case ZVITAG::ZVITAG_MAGNIFICATION:
            return "Objective Magnification";
        case ZVITAG::ZVITAG_OBJECTIVE_NA:
            return "Objective N.A.";
        case ZVITAG::ZVITAG_MICROSCOPE_ILLUMINATION:
            return "Microscope Illumination";
        case ZVITAG::ZVITAG_EXTERNAL_SHUTTER_1:
            return "External Shutter 1";
        case ZVITAG::ZVITAG_EXTERNAL_SHUTTER_2:
            return "External Shutter 2";
        case ZVITAG::ZVITAG_EXTERNAL_SHUTTER_3:
            return "External Shutter 3";
        case ZVITAG::ZVITAG_EXTERNAL_FILTER_WHEEL_1_NAME:
            return "External Filter Wheel 1 Name";
        case ZVITAG::ZVITAG_EXTERNAL_FILTER_WHEEL_2_NAME:
            return "External Filter Wheel 2 Name";
        case ZVITAG::ZVITAG_PARFOCAL_CORRECTION:
            return "Parfocal Correction";
        case ZVITAG::ZVITAG_EXTERNAL_SHUTTER_4:
            return "External Shutter 4";
        case ZVITAG::ZVITAG_EXTERNAL_SHUTTER_5:
            return "External Shutter 5";
        case ZVITAG::ZVITAG_EXTERNAL_SHUTTER_6:
            return "External Shutter 6";
        case ZVITAG::ZVITAG_EXTERNAL_FILTER_WHEEL_3_NAME:
            return "External Filter Wheel 3 Name";
        case ZVITAG::ZVITAG_EXTERNAL_FILTER_WHEEL_4_NAME:
            return "External Filter Wheel 4 Name";
        case ZVITAG::ZVITAG_OBJECTIVE_TURRET_POSITION:
            return "Objective Turret Position";
        case ZVITAG::ZVITAG_OBJECTIVE_CONTRAST_METHOD:
            return "Objective Contrast Method";
        case ZVITAG::ZVITAG_OBJECTIVE_IMMERSION_TYPE:
            return "Objective Immersion Type";
        case ZVITAG::ZVITAG_REFLECTOR_POSITION:
            return "Reflector Position";
        case ZVITAG::ZVITAG_TRANSMITTED_LIGHT_FILTER_1_POS:
            return "Transmitted Light Filter 1 Position";
        case ZVITAG::ZVITAG_TRANSMITTED_LIGHT_FILTER_2_POS:
            return "Transmitted Light Filter 2 Position";
        case ZVITAG::ZVITAG_EXCITATION_FILTER_POSITION:
            return "Excitation Filter Position";
        case ZVITAG::ZVITAG_LAMP_MIRROR_POSITION_OLD:
            return "Lamp Mirror Position (ERSETZT DURCH 241!)";
        case ZVITAG::ZVITAG_EXTERNAL_FILTER_WHEEL_1_POS:
            return "External Filter Wheel 1 Position";
        case ZVITAG::ZVITAG_EXTERNAL_FILTER_WHEEL_2_POS:
            return "External Filter Wheel 2 Position";
        case ZVITAG::ZVITAG_EXTERNAL_FILTER_WHEEL_3_POS:
            return "External Filter Wheel 3 Position";
        case ZVITAG::ZVITAG_EXTERNAL_FILTER_WHEEL_4_POS:
            return "External Filter Wheel 4 Position";
        case ZVITAG::ZVITAG_LIGHTMANAGER_MODE:
            return "Lightmanager Mode";
        case ZVITAG::ZVITAG_HALOGEN_LAMP_CALIBRATION:
            return "Halogen Lamp Calibration";
        case ZVITAG::ZVITAG_CONDENSER_NA_GO_SPEED:
            return "CondenserNAGoSpeed";
        case ZVITAG::ZVITAG_TLF_GO_SPEED:
            return "TransmittedLightFieldstopGoSpeed";
        case ZVITAG::ZVITAG_OPTOVAR_GO_SPEED:
            return "OptovarGoSpeed";
        case ZVITAG::ZVITAG_FOCUS_CALIBRATED:
            return "Focus calibrated";
        case ZVITAG::ZVITAG_FOCUS_BASIC_POSITION:
            return "FocusBasicPosition";
        case ZVITAG::ZVITAG_FOCUS_POWER:
            return "FocusPower";
        case ZVITAG::ZVITAG_FOCUS_BACKLASH:
            return "FocusBacklash";
        case ZVITAG::ZVITAG_FOCUS_MEASUREMENT_ORIGIN:
            return "FocusMeasurementOrigin";
        case ZVITAG::ZVITAG_FOCUS_MEASUREMENT_DISTANCE:
            return "FocusMeasurementDistance";
        case ZVITAG::ZVITAG_FOCUS_SPEED:
            return "FocusSpeed";
        case ZVITAG::ZVITAG_FOCUS_GO_SPEED:
            return "FocusGoSpeed";
        case ZVITAG::ZVITAG_FOCUS_DISTANCE:
            return "Focus Distance";
        case ZVITAG::ZVITAG_FOCUS_INIT_POSITION:
            return "FocusInitPosition";
        case ZVITAG::ZVITAG_STAGE_CALIBRATED:
            return "Stage calibrated";
        case ZVITAG::ZVITAG_STAGE_POWER:
            return "StagePower";
        case ZVITAG::ZVITAG_STAGE_X_BACKLASH:
            return "StageXBacklash";
        case ZVITAG::ZVITAG_STAGE_Y_BACKLASH:
            return "StageYBacklash";
        case ZVITAG::ZVITAG_STAGE_SPEED_X:
            return "Stage Speed X";
        case ZVITAG::ZVITAG_STAGE_SPEED_Y:
            return "Stage Speed Y";
        case ZVITAG::ZVITAG_STAGE_SPEED:
            return "Stage Speed";
        case ZVITAG::ZVITAG_STAGE_GO_SPEED_X:
            return "Stage Go Speed X";
        case ZVITAG::ZVITAG_STAGE_GO_SPEED_Y:
            return "Stage Go Speed Y";
        case ZVITAG::ZVITAG_STAGE_STEP_DISTANCE_X:
            return "Stage Step Distance X";
        case ZVITAG::ZVITAG_STAGE_STEP_DISTANCE_Y:
            return "Stage Step Distance Y";
        case ZVITAG::ZVITAG_STAGE_INIT_POSITION_X:
            return "Stage Initialisation Position X";
        case ZVITAG::ZVITAG_STAGE_INIT_POSITION_Y:
            return "Stage Initialisation Position Y";
        case ZVITAG::ZVITAG_MICROSCOPE_MAGNIFICATION:
            return "MicroscopeMagnification";
        case ZVITAG::ZVITAG_REFLECTOR_MAGNIFICATION:
            return "Reflector Magnification";
        case ZVITAG::ZVITAG_LAMP_MIRROR_POSITION:
            return "Lamp Mirror Position";
        case ZVITAG::ZVITAG_FOCUS_DEPTH:
            return "FocusDepth";
        case ZVITAG::ZVITAG_MICROSCOPE_TYPE:
            return "Microscope Type";
        case ZVITAG::ZVITAG_OBJECTIVE_WORKING_DISTANCE:
            return "Objective Working Distance";
        case ZVITAG::ZVITAG_REFLECTED_LIGHT_APERTURE_GO_SPEED:
            return "ReflectedLightApertureGoSpeed";
        case ZVITAG::ZVITAG_EXTERNAL_SHUTTER:
            return "External Shutter";
        case ZVITAG::ZVITAG_OBJECTIVE_IMMERSION_STOP:
            return "Objective Immersion Stop";
        case ZVITAG::ZVITAG_FOCUS_START_SPEED:
            return "Focus Start Speed";
        case ZVITAG::ZVITAG_FOCUS_ACCELERATION:
            return "Focus Acceleration";
        case ZVITAG::ZVITAG_REFLECTED_LIGHT_FIELDSTOP:
            return "Reflected Light Fieldstop";
        case ZVITAG::ZVITAG_REFLECTED_LIGHT_FIELDSTOP_GO_SPEED:
            return "ReflectedLightFieldstopGoSpeed";
        case ZVITAG::ZVITAG_REFLECTED_LIGHT_FILTER_1:
            return "Reflected Light Filter 1";
        case ZVITAG::ZVITAG_REFLECTED_LIGHT_FILTER_2:
            return "Reflected Light Filter 2";
        case ZVITAG::ZVITAG_REFLECTED_LIGHT_FILTER_1_POS:
            return "Reflected Light Filter 1 Position";
        case ZVITAG::ZVITAG_REFLECTED_LIGHT_FILTER_2_POS:
            return "Reflected Light Filter 2 Position";
        case ZVITAG::ZVITAG_TRANSMITTED_LIGHT_ATTENUATOR:
            return "Transmitted Light Attenuator";
        case ZVITAG::ZVITAG_REFLECTED_LIGHT_ATTENUATOR:
            return "Reflected Light Attenuator";
        case ZVITAG::ZVITAG_TRANSMITTED_LIGHT_SHUTTER:
            return "Transmitted Light Shutter";
        case ZVITAG::ZVITAG_TLA_GO_SPEED:
            return "TransmittedLightAttenuatorGoSpeed";
        case ZVITAG::ZVITAG_RLA_GO_SPEED:
            return "ReflectedLightAttenuatorGoSpeed";
        case ZVITAG::ZVITAG_TLV_FILTER_POSITION:
            return "TransmittedLightVirtualFilterPosition";
        case ZVITAG::ZVITAG_TLV_FILTER:
            return "Transmitted Light Virtual Filter";
        case ZVITAG::ZVITAG_RLV_FILTER_POSITION:
            return "ReflectedLightVirtualFilterPosition";
        case ZVITAG::ZVITAG_RLV_FILTER:
            return "Reflected Light Virtual Filter";
        case ZVITAG::ZVITAG_REFLECTED_LIGHT_HALOGEN_MODE:
            return "Reflected Light Halogen Lamp Mode";
        case ZVITAG::ZVITAG_REFLECTED_LIGHT_HALOGEN_VOLTAGE:
            return "Reflected Light Halogen Lamp Voltage";
        case ZVITAG::ZVITAG_REFLECTED_LIGHT_HALOGEN_COLOR_TEMP:
            return "Reflected Light Halogen Lamp Colour Temperature";
        case ZVITAG::ZVITAG_CONTRASTMANAGER_MODE:
            return "Contrastmanager Mode";
        case ZVITAG::ZVITAG_DAZZLE_PROTECTION_ACTIVE:
            return "Dazzle Protection Active";
        case ZVITAG::ZVITAG_ZOOM:
            return "Zoom";
        case ZVITAG::ZVITAG_ZOOM_GO_SPEED:
            return "ZoomGoSpeed";
        case ZVITAG::ZVITAG_LIGHT_ZOOM:
            return "Light Zoom";
        case ZVITAG::ZVITAG_LIGHT_ZOOM_GO_SPEED:
            return "LightZoomGoSpeed";
        case ZVITAG::ZVITAG_LIGHTZOOM_COUPLED:
            return "Lightzoom Coupled";
        case ZVITAG::ZVITAG_TRANSMITTED_LIGHT_HALOGEN_MODE:
            return "Transmitted Light Halogen Lamp Mode";
        case ZVITAG::ZVITAG_TRANSMITTED_LIGHT_HALOGEN_VOLTAGE:
            return "Transmitted Light Halogen Lamp Voltage";
        case ZVITAG::ZVITAG_TRANSMITTED_LIGHT_HALOGEN_COLOR_TEMP:
            return "Transmitted Light Halogen Lamp Colour Temperature";
        case ZVITAG::ZVITAG_REFLECTED_COLDLIGHT_MODE:
            return "Reflected Coldlight Mode";
        case ZVITAG::ZVITAG_REFLECTED_COLDLIGHT_INTENSITY:
            return "Reflected Coldlight Intensity";
        case ZVITAG::ZVITAG_REFLECTED_COLDLIGHT_COLOR_TEMP:
            return "Reflected Coldlight Colour Temperature";
        case ZVITAG::ZVITAG_TRANSMITTED_COLDLIGHT_MODE:
            return "Transmitted Coldlight Mode";
        case ZVITAG::ZVITAG_TRANSMITTED_COLDLIGHT_INTENSITY:
            return "Transmitted Coldlight Intensity";
        case ZVITAG::ZVITAG_TRANSMITTED_COLDLIGHT_COLOR_TEMP:
            return "Transmitted Coldlight Colour Temperature";
        case ZVITAG::ZVITAG_INFINITYSPACE_PORTCHANGER_POSITION:
            return "Infinityspace Portchanger Position";
        case ZVITAG::ZVITAG_BEAMSPLITTER_INFINITY_SPACE:
            return "Beamsplitter Infinity Space";
        case ZVITAG::ZVITAG_TWOTV_VISCAMCHANGER_POSITION:
            return "TwoTv VisCamChanger Position";
        case ZVITAG::ZVITAG_BEAMSPLITTER_OCULAR:
            return "Beamsplitter Ocular";
        case ZVITAG::ZVITAG_TWOTV_CAMERASCHANGER_POSITION:
            return "TwoTv CamerasChanger Position";
        case ZVITAG::ZVITAG_BEAMSPLITTER_CAMERAS:
            return "Beamsplitter Cameras";
        case ZVITAG::ZVITAG_OCULAR_SHUTTER:
            return "Ocular Shutter";
        case ZVITAG::ZVITAG_TWOTV_CAMERASCHANGER_CUBE:
            return "TwoTv CamerasChanger Cube";
        case ZVITAG::ZVITAG_LIGHT_WAVELENGTH:
            return "LightWaveLength";
        case ZVITAG::ZVITAG_OCULAR_MAGNIFICATION:
            return "Ocular Magnification";
        case ZVITAG::ZVITAG_CAMERA_ADAPTER_MAGNIFICATION:
            return "Camera Adapter Magnification";
        case ZVITAG::ZVITAG_MICROSCOPE_PORT:
            return "Microscope Port";
        case ZVITAG::ZVITAG_OCULAR_TOTAL_MAGNIFICATION:
            return "Ocular Total Magnification";
        case ZVITAG::ZVITAG_FIELD_OF_VIEW:
            return "Field of View";
        case ZVITAG::ZVITAG_OCULAR:
            return "Ocular";
        case ZVITAG::ZVITAG_CAMERA_ADAPTER:
            return "CameraAdapter";
        case ZVITAG::ZVITAG_STAGE_JOYSTICK_ENABLED:
            return "StageJoystickEnabled";
        case ZVITAG::ZVITAG_CONTRASTMANAGER_CONTRAST_METHOD:
            return "ContrastmanagerContrastMethod";
        case ZVITAG::ZVITAG_CAMERASCHANGER_BEAMSPLITTER_TYPE:
            return "CamerasChanger BeamSplitter Type";
        case ZVITAG::ZVITAG_REARPORT_SLIDER_POSITION:
            return "Rearport Slider Position";
        case ZVITAG::ZVITAG_REARPORT_SOURCE:
            return "Rearport Source";
        case ZVITAG::ZVITAG_BEAMSPLITTER_TYPE_INFINITY_SPACE:
            return "Beamsplitter Type Infinity Space";
        case ZVITAG::ZVITAG_FLUORESCENCE_ATTENUATOR:
            return "Fluorescence Attenuator";
        case ZVITAG::ZVITAG_FLUORESCENCE_ATTENUATOR_POSITION:
            return "Fluorescence Attenuator Position";
        case ZVITAG::ZVITAG_CAMERA_FRAMESTART_LEFT:
            return "Camera Framestart Left";
        case ZVITAG::ZVITAG_CAMERA_FRAMESTART_TOP:
            return "Camera Framestart Top";
        case ZVITAG::ZVITAG_CAMERA_FRAME_WIDTH:
            return "Camera Frame Width";
        case ZVITAG::ZVITAG_CAMERA_FRAME_HEIGHT:
            return "Camera Frame Height";
        case ZVITAG::ZVITAG_CAMERA_BINNING:
            return "Camera Binning";
        case ZVITAG::ZVITAG_CAMERA_FRAME_FULL:
            return "CameraFrameFull";
        case ZVITAG::ZVITAG_CAMERA_FRAME_PIXEL_DISTANCE:
            return "CameraFramePixelDistance";
        case ZVITAG::ZVITAG_DATA_FORMAT_USE_SCALING:
            return "DataFormatUseScaling";
        case ZVITAG::ZVITAG_CAMERA_FRAME_IMAGE_ORIENTATION:
            return "CameraFrameImageOrientation";
        case ZVITAG::ZVITAG_VIDEO_MONOCHROME_SIGNAL_TYPE:
            return "VideoMonochromeSignalType";
        case ZVITAG::ZVITAG_VIDEO_COLOR_SIGNAL_TYPE:
            return "VideoColorSignalType";
        case ZVITAG::ZVITAG_METEOR_CHANNEL_INPUT:
            return "MeteorChannelInput";
        case ZVITAG::ZVITAG_METEOR_CHANNEL_SYNC:
            return "MeteorChannelSync";
        case ZVITAG::ZVITAG_WHITE_BALANCE_ENABLED:
            return "WhiteBalanceEnabled";
        case ZVITAG::ZVITAG_CAMERA_WHITE_BALANCE_RED:
            return "CameraWhiteBalanceRed";
        case ZVITAG::ZVITAG_CAMERA_WHITE_BALANCE_GREEN:
            return "CameraWhiteBalanceGreen";
        case ZVITAG::ZVITAG_CAMERA_WHITE_BALANCE_BLUE:
            return "CameraWhiteBalanceBlue";
        case ZVITAG::ZVITAG_CAMERA_FRAME_SCALING_FACTOR:
            return "CameraFrameScalingFactor";
        case ZVITAG::ZVITAG_METEOR_CAMERA_TYPE:
            return "Meteor Camera Type";
        case ZVITAG::ZVITAG_EXPOSURE_TIME:
            return "Exposure Time [ms]";
        case ZVITAG::ZVITAG_CAMERA_EXPOSURE_TIME_AUTO_CALC:
            return "CameraExposureTimeAutoCalculate";
        case ZVITAG::ZVITAG_METEOR_GAIN_VALUE:
            return "Meteor Gain Value";
        case ZVITAG::ZVITAG_METEOR_GAIN_AUTOMATIC:
            return "Meteor Gain Automatic";
        case ZVITAG::ZVITAG_METEOR_ADJUST_HUE:
            return "MeteorAdjustHue";
        case ZVITAG::ZVITAG_METEOR_ADJUST_SATURATION:
            return "MeteorAdjustSaturation";
        case ZVITAG::ZVITAG_METEOR_ADJUST_RED_LOW:
            return "MeteorAdjustRedLow";
        case ZVITAG::ZVITAG_METEOR_ADJUST_GREEN_LOW:
            return "MeteorAdjustGreenLow";
        case ZVITAG::ZVITAG_METEOR_BLUE_LOW:
            return "Meteor Blue Low";
        case ZVITAG::ZVITAG_METEOR_ADJUST_RED_HIGH:
            return "MeteorAdjustRedHigh";
        case ZVITAG::ZVITAG_METEOR_ADJUST_GREEN_HIGH:
            return "MeteorAdjustGreenHigh";
        case ZVITAG::ZVITAG_METEOR_BLUE_HIGH:
            return "Meteor Blue High";
        case ZVITAG::ZVITAG_CAMERA_EXPOSURE_TIME_CALC_CONTROL:
            return "CameraExposureTimeCalculationControl";
        case ZVITAG::ZVITAG_AXIOCAM_FADING_CORRECTION_ENABLE:
            return "AxioCamFadingCorrectionEnable";
        case ZVITAG::ZVITAG_CAMERA_LIVE_IMAGE:
            return "CameraLiveImage";
        case ZVITAG::ZVITAG_CAMERA_LIVE_ENABLED:
            return "CameraLiveEnabled";
        case ZVITAG::ZVITAG_LIVE_IMAGE_SYNC_OBJECT_NAME:
            return "LiveImageSyncObjectName";
        case ZVITAG::ZVITAG_CAMERA_LIVE_SPEED:
            return "CameraLiveSpeed";
        case ZVITAG::ZVITAG_CAMERA_IMAGE:
            return "CameraImage";
        case ZVITAG::ZVITAG_CAMERA_IMAGE_WIDTH:
            return "CameraImageWidth";
        case ZVITAG::ZVITAG_CAMERA_IMAGE_HEIGHT:
            return "CameraImageHeight";
        case ZVITAG::ZVITAG_CAMERA_IMAGE_PIXEL_TYPE:
            return "CameraImagePixelType";
        case ZVITAG::ZVITAG_CAMERA_IMAGE_SH_MEMORY_NAME:
            return "CameraImageShMemoryName";
        case ZVITAG::ZVITAG_CAMERA_LIVE_IMAGE_WIDTH:
            return "CameraLiveImageWidth";
        case ZVITAG::ZVITAG_CAMERA_LIVE_IMAGE_HEIGHT:
            return "CameraLiveImageHeight";
        case ZVITAG::ZVITAG_CAMERA_LIVE_IMAGE_PIXEL_TYPE:
            return "CameraLiveImagePixelType";
        case ZVITAG::ZVITAG_CAMERA_LIVE_IMAGE_SH_MEMORY_NAME:
            return "CameraLiveImageShMemoryName";
        case ZVITAG::ZVITAG_CAMERA_LIVE_MAXIMUM_SPEED:
            return "CameraLiveMaximumSpeed";
        case ZVITAG::ZVITAG_CAMERA_LIVE_BINNING:
            return "CameraLiveBinning";
        case ZVITAG::ZVITAG_CAMERA_LIVE_GAIN_VALUE:
            return "CameraLiveGainValue";
        case ZVITAG::ZVITAG_CAMERA_LIVE_EXPOSURE_TIME_VALUE:
            return "CameraLiveExposureTimeValue";
        case ZVITAG::ZVITAG_CAMERA_LIVE_SCALING_FACTOR:
            return "CameraLiveScalingFactor";
        case ZVITAG::ZVITAG_IMAGE_INDEX_U:
            return "Image Index U";
        case ZVITAG::ZVITAG_IMAGE_INDEX_V:
            return "Image Index V";
        case ZVITAG::ZVITAG_IMAGE_INDEX_Z:
            return "Image Index Z";
        case ZVITAG::ZVITAG_IMAGE_INDEX_C:
            return "ImageIndex C";
        case ZVITAG::ZVITAG_IMAGE_INDEX_T:
            return "Image Index T";
        case ZVITAG::ZVITAG_IMAGE_TILE_INDEX:
            return "Image Tile Index";
        case ZVITAG::ZVITAG_IMAGE_ACQUSITION_INDEX:
            return "Image acquisition Index";
        case ZVITAG::ZVITAG_IMAGE_COUNT_TILES:
            return "ImageCount Tiles";
        case ZVITAG::ZVITAG_IMAGE_COUNT_A:
            return "ImageCount A";
        case ZVITAG::ZVITAG_IMAGE_INDEX_S:
            return "ImageIndex S";
        case ZVITAG::ZVITAG_IMAGE_INDEX_RAW:
            return "Image Index Raw";
        case ZVITAG::ZVITAG_IMAGE_COUNT_Z:
            return "Image Count Z";
        case ZVITAG::ZVITAG_IMAGE_COUNT_C:
            return "Image Count C";
        case ZVITAG::ZVITAG_IMAGE_COUNT_T:
            return "Image Count T";
        case ZVITAG::ZVITAG_IMAGE_COUNT_U:
            return "Image Count U";
        case ZVITAG::ZVITAG_IMAGE_COUNT_V:
            return "Image Count V";
        case ZVITAG::ZVITAG_IMAGE_COUNT_S:
            return "Image Count S";
        case ZVITAG::ZVITAG_ORIGINAL_STAGE_POSITION_X:
            return "Original Stage Position X";
        case ZVITAG::ZVITAG_ORIGINAL_STAGE_POSITION_Y:
            return "Original Stage Position Y";
        case ZVITAG::ZVITAG_LAYER_DRAW_FLAGS:
            return "LayerDrawFlags";
        case ZVITAG::ZVITAG_REMAINING_TIME:
            return "Remaining Time";
        case ZVITAG::ZVITAG_USER_FIELD_1:
            return "User Field 1";
        case ZVITAG::ZVITAG_USER_FIELD_2:
            return "User Field 2";
        case ZVITAG::ZVITAG_USER_FIELD_3:
            return "User Field 3";
        case ZVITAG::ZVITAG_USER_FIELD_4:
            return "User Field 4";
        case ZVITAG::ZVITAG_USER_FIELD_5:
            return "User Field 5";
        case ZVITAG::ZVITAG_USER_FIELD_6:
            return "User Field 6";
        case ZVITAG::ZVITAG_USER_FIELD_7:
            return "User Field 7";
        case ZVITAG::ZVITAG_USER_FIELD_8:
            return "User Field 8";
        case ZVITAG::ZVITAG_USER_FIELD_9:
            return "User Field 9";
        case ZVITAG::ZVITAG_USER_FIELD_10:
            return "User Field 10";
        case ZVITAG::ZVITAG_ID:
            return "ID";
        case ZVITAG::ZVITAG_NAME:
            return "Name";
        case ZVITAG::ZVITAG_VALUE:
            return "Value";
        case ZVITAG::ZVITAG_PVCAM_CLOCKING_MODE:
            return "PvCamClockingMode";
        case ZVITAG::ZVITAG_AUTOFOCUS_STATUS_REPORT:
            return "Autofocus Status Report";
        case ZVITAG::ZVITAG_AUTOFOCUS_POSITION:
            return "Autofocus Position";
        case ZVITAG::ZVITAG_AUTOFOCUS_POSITION_OFFSET:
            return "Autofocus Position Offset";
        case ZVITAG::ZVITAG_AUTOFOCUS_EMPTY_FIELD_THRESHOLD:
            return "Autofocus Empty Field Threshold";
        case ZVITAG::ZVITAG_AUTOFOCUS_CALIBRATION_NAME:
            return "Autofocus Calibration Name";
        case ZVITAG::ZVITAG_AUTOFOCUS_CURRENT_CALIBRATION_ITEM:
            return "Autofocus Current Calibration Item";
        case ZVITAG::ZVITAG_CAMERA_FRAME_FULL_WIDTH:
            return "CameraFrameFullWidth";
        case ZVITAG::ZVITAG_CAMERA_FRAME_FULL_HEIGHT:
            return "CameraFrameFullHeight";
        case ZVITAG::ZVITAG_AXIOCAM_SHUTTER_SIGNAL:
            return "AxioCam Shutter Signal";
        case ZVITAG::ZVITAG_AXIOCAM_DELAY_TIME:
            return "AxioCam Delay Time";
        case ZVITAG::ZVITAG_AXIOCAM_SHUTTER_CONTROL:
            return "AxioCam Shutter Control";
        case ZVITAG::ZVITAG_AXIOCAM_BLACK_REF_IS_CALCULATED:
            return "AxioCamBlackRefIsCalculated";
        case ZVITAG::ZVITAG_AXIOCAM_BLACK_REFERENCE:
            return "AxioCam Black Reference";
        case ZVITAG::ZVITAG_CAMERA_SHADING_CORRECTION:
            return "Camera Shading Correction";
        case ZVITAG::ZVITAG_AXIOCAM_ENHANCE_COLOR:
            return "AxioCam Enhance Color";
        case ZVITAG::ZVITAG_AXIOCAM_NIR_MODE:
            return "AxioCam NIR Mode";
        case ZVITAG::ZVITAG_CAMERA_SHUTTER_CLOSE_DELAY:
            return "CameraShutterCloseDelay";
        case ZVITAG::ZVITAG_CAMERA_WHITE_BALANCE_AUTO_CALC:
            return "CameraWhiteBalanceAutoCalculate";
        case ZVITAG::ZVITAG_AXIOCAM_NIR_MODE_AVAILABLE:
            return "AxioCamNIRModeAvailable";
        case ZVITAG::ZVITAG_AXIOCAM_FADING_CORRECTION_AVAILABLE:
            return "AxioCamFadingCorrectionAvailable";
        case ZVITAG::ZVITAG_AXIOCAM_ENHANCE_COLOR_AVAILABLE:
            return "AxioCamEnhanceColorAvailable";
        case ZVITAG::ZVITAG_METEOR_VIDEO_NORM:
            return "MeteorVideoNorm";
        case ZVITAG::ZVITAG_METEOR_ADJUST_WHITE_REFERENCE:
            return "MeteorAdjustWhiteReference";
        case ZVITAG::ZVITAG_METEOR_BLACK_REFERENCE:
            return "Meteor Black Reference";
        case ZVITAG::ZVITAG_METEOR_CHANNEL_INPUT_COUNT_MONO:
            return "MeteorChannelInputCountMono";
        case ZVITAG::ZVITAG_METEOR_CHANNEL_INPUT_COUNT_RGB:
            return "MeteorChannelInputCountRGB";
        case ZVITAG::ZVITAG_METEOR_ENABLE_VCR:
            return "MeteorEnableVCR";
        case ZVITAG::ZVITAG_METEOR_BRIGHTNESS:
            return "Meteor Brightness";
        case ZVITAG::ZVITAG_METEOR_CONTRAST:
            return "Meteor Contrast";
        case ZVITAG::ZVITAG_AXIOCAM_SELECTOR:
            return "AxioCamSelector";
        case ZVITAG::ZVITAG_AXIOCAM_TYPE:
            return "AxioCam Type";
        case ZVITAG::ZVITAG_AXIOCAM_INFO:
            return "AxioCamInfo";
        case ZVITAG::ZVITAG_AXIOCAM_RESOLUTION:
            return "AxioCam Resolution";
        case ZVITAG::ZVITAG_AXIOCAM_COLOUR_MODEL:
            return "AxioCam Colour Model";
        case ZVITAG::ZVITAG_AXIOCAM_MICROSCANNING:
            return "AxioCamMicroScanning";
        case ZVITAG::ZVITAG_AMPLIFICATION_INDEX:
            return "Amplification Index";
        case ZVITAG::ZVITAG_DEVICE_COMMAND:
            return "DeviceCommand";
        case ZVITAG::ZVITAG_BEAM_LOCATION:
            return "BeamLocation";
        case ZVITAG::ZVITAG_COMPONENT_TYPE:
            return "ComponentType";
        case ZVITAG::ZVITAG_CONTROLLER_TYPE:
            return "ControllerType";
        case ZVITAG::ZVITAG_CAMERA_WB_CALC_RED_PAINT:
            return "CameraWhiteBalanceCalculationRedPaint";
        case ZVITAG::ZVITAG_CAMERA_WB_CALC_BLUE_PAINT:
            return "CameraWhiteBalanceCalculationBluePaint";
        case ZVITAG::ZVITAG_CAMERA_WB_SET_RED:
            return "CameraWhiteBalanceSetRed";
        case ZVITAG::ZVITAG_CAMERA_WB_SET_GREEN:
            return "CameraWhiteBalanceSetGreen";
        case ZVITAG::ZVITAG_CAMERA_WB_SET_BLUE:
            return "CameraWhiteBalanceSetBlue";
        case ZVITAG::ZVITAG_CAMERA_WB_SET_TARGET_RED:
            return "CameraWhiteBalanceSetTargetRed";
        case ZVITAG::ZVITAG_CAMERA_WB_SET_TARGET_GREEN:
            return "CameraWhiteBalanceSetTargetGreen";
        case ZVITAG::ZVITAG_CAMERA_WB_SET_TARGET_BLUE:
            return "CameraWhiteBalanceSetTargetBlue";
        case ZVITAG::ZVITAG_APOTOMECAM_CALIBRATION_MODE:
            return "ApotomeCamCalibrationMode";
        case ZVITAG::ZVITAG_APOTOME_GRID_POSITION:
            return "ApoTome Grid Position";
        case ZVITAG::ZVITAG_APOTOMECAM_SCANNER_POSITION:
            return "ApotomeCamScannerPosition";
        case ZVITAG::ZVITAG_APOTOME_FULL_PHASE_SHIFT:
            return "ApoTome Full Phase Shift";
        case ZVITAG::ZVITAG_APOTOME_GRID_NAME:
            return "ApoTome Grid Name";
        case ZVITAG::ZVITAG_APOTOME_STAINING:
            return "ApoTome Staining";
        case ZVITAG::ZVITAG_APOTOME_PROCESSING_MODE:
            return "ApoTome Processing Mode";
        case ZVITAG::ZVITAG_APOTOMECAM_LIVE_COMBINE_MODE:
            return "ApotomeCamLiveCombineMode";
        case ZVITAG::ZVITAG_APOTOME_FILTER_NAME:
            return "ApoTome Filter Name";
        case ZVITAG::ZVITAG_APOTOME_FILTER_STRENGTH:
            return "Apotome Filter Strength";
        case ZVITAG::ZVITAG_APOTOMECAM_FILTER_HARMONICS:
            return "ApotomeCamFilterHarmonics";
        case ZVITAG::ZVITAG_APOTOME_GRATING_PERIOD:
            return "ApoTome Grating Period";
        case ZVITAG::ZVITAG_APOTOME_AUTO_SHUTTER_USED:
            return "ApoTome Auto Shutter Used";
        case ZVITAG::ZVITAG_APOTOMECAM_STATUS:
            return "ApotomeCamStatus";
        case ZVITAG::ZVITAG_APOTOMECAM_NORMALIZE:
            return "ApotomeCamNormalize";
        case ZVITAG::ZVITAG_APOTOMECAM_SETTINGS_MANAGER:
            return "ApotomeCamSettingsManager";
        case ZVITAG::ZVITAG_DEEPVIEWCAM_SUPERVISOR_MODE:
            return "DeepviewCamSupervisorMode";
        case ZVITAG::ZVITAG_DEEPVIEW_PROCESSING:
            return "DeepView Processing";
        case ZVITAG::ZVITAG_DEEPVIEWCAM_FILTER_NAME:
            return "DeepviewCamFilterName";
        case ZVITAG::ZVITAG_DEEPVIEWCAM_STATUS:
            return "DeepviewCamStatus";
        case ZVITAG::ZVITAG_DEEPVIEWCAM_SETTINGS_MANAGER:
            return "DeepviewCamSettingsManager";
        case ZVITAG::ZVITAG_DEVICE_SCALING_NAME:
            return "DeviceScalingName";
        case ZVITAG::ZVITAG_CAMERA_SHADING_IS_CALCULATED:
            return "CameraShadingIsCalculated";
        case ZVITAG::ZVITAG_CAMERA_SHADING_CALCULATION_NAME:
            return "CameraShadingCalculationName";
        case ZVITAG::ZVITAG_CAMERA_SHADING_AUTO_CALCULATE:
            return "CameraShadingAutoCalculate";
        case ZVITAG::ZVITAG_CAMERA_TRIGGER_AVAILABLE:
            return "CameraTriggerAvailable";
        case ZVITAG::ZVITAG_CAMERA_SHUTTER_AVAILABLE:
            return "CameraShutterAvailable";
        case ZVITAG::ZVITAG_AXIOCAM_SHUTTER_MICROSCAN_ENABLE:
            return "AxioCamShutterMicroScanningEnable";
        case ZVITAG::ZVITAG_APOTOMECAM_LIVE_FOCUS:
            return "ApotomeCamLiveFocus";
        case ZVITAG::ZVITAG_DEVICE_INIT_STATUS:
            return "DeviceInitStatus";
        case ZVITAG::ZVITAG_DEVICE_ERROR_STATUS:
            return "DeviceErrorStatus";
        case ZVITAG::ZVITAG_APOTOMECAM_SLIDER_IN_GRID_POSITION:
            return "ApotomeCamSliderInGridPosition";
        case ZVITAG::ZVITAG_ORCA_NIR_MODE_USED:
            return "Orca NIR Mode Used";
        case ZVITAG::ZVITAG_ORCA_ANALOG_GAIN:
            return "Orca Analog Gain";
        case ZVITAG::ZVITAG_ORCA_ANALOG_OFFSET:
            return "Orca Analog Offset";
        case ZVITAG::ZVITAG_ORCA_BINNING:
            return "Orca Binning";
        case ZVITAG::ZVITAG_ORCA_BIT_DEPTH:
            return "Orca Bit Depth";
        case ZVITAG::ZVITAG_APOTOME_AVERAGING_COUNT:
            return "ApoTome Averaging Count";
        case ZVITAG::ZVITAG_DEEPVIEW_DOF:
            return "DeepView DoF";
        case ZVITAG::ZVITAG_DEEPVIEW_EDOF:
            return "DeepView EDoF";
        case ZVITAG::ZVITAG_DEEPVIEW_SLIDER_NAME:
            return "DeepView Slider Name";
        case ZVITAG::ZVITAG_EXCITATION_WAVELENGTH:
            return "ExcitationWavelength";
        case ZVITAG::ZVITAG_EMISSION_WAVELENGTH:
            return "EmissionWavelength";
        }
        return nullptr;
    }

} // namespace slideio
