// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/dcm/dcmfile.hpp"
#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmimgle/dcmimage.h>
#include <boost/format.hpp>
#include "slideio/base.hpp"
#include "slideio/core/cvtools.hpp"

#include <ostream>

#include "slideio/structs.hpp"
#include "slideio/imagetools/imagetools.hpp"

using namespace slideio;

static std::string photoInterpetationToString(EPhotoInterpetation photoInt)
{
#define TONAME(name) std::string(#name)

    switch (photoInt)
    {
    case EPhotoInterpetation::PHIN_UNKNOWN: return TONAME(PHIN_UNKNOWN);
    case EPhotoInterpetation::PHIN_MONOCHROME1: return TONAME(PHIN_MONOCHROME1);
    case EPhotoInterpetation::PHIN_MONOCHROME2: return TONAME(PHIN_MONOCHROME2);
    case EPhotoInterpetation::PHIN_RGB: return TONAME(PHIN_RGB);
    case EPhotoInterpetation::PHIN_PALETTE: return TONAME(PHIN_PALETTE);
    case EPhotoInterpetation::PHIN_YCBCR: return TONAME(PHIN_YCBCR);
    case EPhotoInterpetation::PHIN_YBR_FULL: return TONAME(PHIN_YBR_FULL);
    case EPhotoInterpetation::PHIN_YBR_422_FULL: return TONAME(PHIN_YBR_422_FULL);
    case EPhotoInterpetation::PHIN_HSV: return TONAME(PHIN_HSV);
    case EPhotoInterpetation::PHIN_ARGB: return TONAME(PHIN_ARGB);
    case EPhotoInterpetation::PHIN_CMYK: return TONAME(PHIN_CMYK);
    case EPhotoInterpetation::PHIN_YBR_FULL_422: return TONAME(PHIN_YBR_FULL_422);
    case EPhotoInterpetation::PHIN_YBR_PARTIAL_420: return TONAME(PHIN_YBR_PARTIAL_420);
    case EPhotoInterpetation::PHIN_YBR_ICT: return TONAME(PHIN_YBR_ICT);
    case EPhotoInterpetation::PHIN_YBR_RCT: return TONAME(PHIN_YBR_RCT);
    default: ;
    }
    RAISE_RUNTIME_ERROR << "Unexpected photointerpretation: "
        << static_cast<int>(photoInt);
}

DCMFile::DCMFile(const std::string& filePath):
    m_filePath(filePath)
{
    m_file.reset(new DcmFileFormat);
}

void DCMFile::init()
{
    SLIDEIO_LOG(trace) << "DCMFlile::init: initializing DICOM file " << m_filePath;

    OFCondition status = m_file->loadFile(m_filePath.c_str());
    if (status.bad())
    {
        RAISE_RUNTIME_ERROR << "DCMImageDriver: Cannot open file: " << m_filePath;
    }
    DcmDataset* dataset = getValidDataset();

    if (!getIntTag(DCM_Columns, m_width))
    {
        RAISE_RUNTIME_ERROR << "DCMImageDriver: Cannot extract image width for the file:" << m_filePath;
    }
    if (!getIntTag(DCM_Rows, m_height))
    {
        RAISE_RUNTIME_ERROR << "DCMImageDriver: Cannot extract image height for the file:" << m_filePath;
    }
    if (!getIntTag(DCM_NumberOfFrames, m_slices))
    {
        m_slices = 1;
    }
    if (!getStringTag(DCM_SeriesInstanceUID, m_seriesUID))
    {
        m_seriesUID = "Unknown series";
    }
    if (!getIntTag(DCM_InstanceNumber, m_instanceNumber))
    {
        m_instanceNumber = -1;
    }
    if (!getIntTag(DCM_SamplesPerPixel, m_numChannels))
    {
        m_numChannels = 1;
    }
    m_useWindowing = getDblTag(DCM_WindowCenter, m_windowCenter, -1.) &&
        getDblTag(DCM_WindowWidth, m_windowWidth, -1.);
    m_useRescaling = getDblTag(DCM_RescaleSlope, m_rescaleSlope, -1.) &&
        getDblTag(DCM_RescaleIntercept, m_rescaleIntercept, -1.);

    getStringTag(DCM_SeriesDescription, m_seriesDescription);
    int bitsAllocated(0);
    if (!getIntTag(DCM_BitsAllocated, bitsAllocated))
    {
        RAISE_RUNTIME_ERROR << "DCMImageDriver: undefined valude for DCM_BitsAllocated tag. File:" << m_filePath;
    }

    int pixelRepresentation(0);
    if (!getIntTag(DCM_PixelRepresentation, pixelRepresentation))
    {
        RAISE_RUNTIME_ERROR << "DCMImageDriver: undefined valude for DCM_PixelRepresentation tag. File:" << m_filePath;
    }
    const int PXREP_SIGNED = 1;
    const int PXREP_UNSIGNED = 0;
    if (bitsAllocated == 8)
    {
        m_dataType = pixelRepresentation == PXREP_SIGNED ? DataType::DT_Int8 : DataType::DT_Byte;
    }
    else if (bitsAllocated == 16)
    {
        m_dataType = pixelRepresentation == PXREP_SIGNED ? DataType::DT_Int16 : DataType::DT_UInt16;
    }
    else
    {
        RAISE_RUNTIME_ERROR << "DCMImageDriver: unexpected value for allocated bits: " << bitsAllocated;
    }
    int planarConfiguration(0);
    if (!getIntTag(DCM_PlanarConfiguration, planarConfiguration))
    {
        planarConfiguration = 0;
    }
    m_planarConfiguration = planarConfiguration == 1;
    initPhotoInterpretaion();
    logData();
    defineCompression();
}

void DCMFile::defineCompression()
{
    const E_TransferSyntax xfer = getValidDataset()->getOriginalXfer();
    switch (xfer)
    {
    case EXS_LittleEndianImplicit:
    case EXS_BigEndianImplicit:
    case EXS_LittleEndianExplicit:
    case EXS_BigEndianExplicit:
        m_compression = Compression::Uncompressed;
        break;
    case EXS_JPEGProcess1:
    case EXS_JPEGProcess2_4:
    case EXS_JPEGProcess3_5:
    case EXS_JPEGProcess6_8:
    case EXS_JPEGProcess7_9:
    case EXS_JPEGProcess10_12:
    case EXS_JPEGProcess11_13:
    case EXS_JPEGProcess14:
    case EXS_JPEGProcess15:
    case EXS_JPEGProcess16_18:
    case EXS_JPEGProcess17_19:
    case EXS_JPEGProcess20_22:
    case EXS_JPEGProcess21_23:
    case EXS_JPEGProcess24_26:
    case EXS_JPEGProcess25_27:
    case EXS_JPEGProcess28:
    case EXS_JPEGProcess29:
    case EXS_JPEGProcess14SV1:
    case EXS_JPEGLSLossless:
    case EXS_JPEGLSLossy:
    case EXS_JPEG2000:
    case EXS_JPEG2000MulticomponentLosslessOnly:
    case EXS_JPEG2000Multicomponent:
        m_compression = Compression::Jpeg;
        break;
    case EXS_RLELossless:
        m_compression = Compression::RLE;
        break;
    case EXS_DeflatedLittleEndianExplicit:
        m_compression = Compression::Zlib;
        break;
    case EXS_JPEG2000LosslessOnly:
        m_compression = Compression::Jpeg2000;
        break;
    default: ;
        SLIDEIO_LOG(warning) << "DCMImageDriver: Unknown xTransfer:" << xfer << " for file " << m_filePath;
    }
}

void DCMFile::initPhotoInterpretaion()
{
    std::string photoInt;
    if (getStringTag(DCM_PhotometricInterpretation, photoInt))
    {
        if (photoInt == "MONOCHROME1")
        {
            m_photoInterpretation = EPhotoInterpetation::PHIN_MONOCHROME1;
        }
        else if (photoInt == "MONOCHROME2")
        {
            m_photoInterpretation = EPhotoInterpetation::PHIN_MONOCHROME2;
        }
        else if (photoInt == "PALETTE COLOR")
        {
            m_photoInterpretation = EPhotoInterpetation::PHIN_PALETTE;
        }
        else if (photoInt == "RGB")
        {
            m_photoInterpretation = EPhotoInterpetation::PHIN_RGB;
        }
        else if (photoInt == "HSV")
        {
            m_photoInterpretation = EPhotoInterpetation::PHIN_HSV;
        }
        else if (photoInt == "ARGB")
        {
            m_photoInterpretation = EPhotoInterpetation::PHIN_ARGB;
        }
        else if (photoInt == "CMYK")
        {
            m_photoInterpretation = EPhotoInterpetation::PHIN_CMYK;
        }
        else if (photoInt == "YBR_FULL")
        {
            m_photoInterpretation = EPhotoInterpetation::PHIN_YBR_FULL;
        }
        else if (photoInt == "YCBCR")
        {
            m_photoInterpretation = EPhotoInterpetation::PHIN_YCBCR;
        }
        else if (photoInt == "YBR_FULL_422")
        {
            m_photoInterpretation = EPhotoInterpetation::PHIN_YBR_FULL_422;
        }
        else if (photoInt == "YBR_PARTIAL_420")
        {
            m_photoInterpretation = EPhotoInterpetation::PHIN_YBR_PARTIAL_420;
        }
        else if (photoInt == "YBR_ICT")
        {
            m_photoInterpretation = EPhotoInterpetation::PHIN_YBR_ICT;
        }
        else if (photoInt == "YBR_RCT")
        {
            m_photoInterpretation = EPhotoInterpetation::PHIN_YBR_RCT;
        }
        else
        {
            RAISE_RUNTIME_ERROR << "DCMImageDriver: Unexpected photointerpretation:" << photoInt
                << " for file " << m_filePath;
        }
    }
}

void DCMFile::logData()
{
    SLIDEIO_LOG(info) << "DICOM file: " << m_filePath << std::endl
        << "Width:" << m_width << std::endl
        << "Height:" << m_height << std::endl
        << "Slices:" << m_slices << std::endl
        << "Series UID:" << m_seriesUID << std::endl
        << "Series description:" << m_seriesDescription << std::endl
        << "Channels:" << m_numChannels << std::endl
        << "Data type:" << CVTools::dataTypeToString(m_dataType) << std::endl
        << "Planar configuration:" << m_planarConfiguration << std::endl
        << "Photointerpretation:" << photoInterpetationToString(m_photoInterpretation) << std::endl
        << "Compression:" << CVTools::compressionToString(m_compression) << std::endl
        << "Window center:" << m_windowCenter << std::endl
        << "Window width" << m_windowWidth << std::endl
        << "Slope:" << m_rescaleSlope << std::endl
        << "Intercept:" << m_rescaleIntercept << std::endl;
}

void DCMFile::readPixelValues(std::vector<cv::Mat>& frames, int startFrame, int numFrames)
{
    SLIDEIO_LOG(trace) << "Extracting pixel values from the dataset";

    DcmDataset* dataset = getDataset();
    if (!dataset)
    {
        RAISE_RUNTIME_ERROR << "DCMImageDriver: unexpected null as dataset for file " << m_filePath;
    }

    DicomImage image(dataset, EXS_LittleEndianExplicit, CIF_UsePartialAccessToPixelData, (ulong)startFrame, (ulong)1);
    if(image.getStatus()!=EIS_Normal) 
    {
        RAISE_RUNTIME_ERROR << "DCMImageDriver: cannot decompress file " << m_filePath << ". Image status: " << image.getStatus();
    }

    const int numFileFrames = image.getFrameCount();
    const int numChannels = getNumChannels();
    const DataType originalDataType = getDataType();
    DataType intermediateDataType = originalDataType;
    const int cvIntermediateType = CVTools::toOpencvType(intermediateDataType);
    const int cvOriginalType = CVTools::toOpencvType(originalDataType);
    const int numFramePixels = getWidth() * getHeight();
    const int numFrameBytes = numChannels * numFramePixels * ImageTools::dataTypeSize(getDataType());

    if (m_useRescaling)
    {
        if (originalDataType == DataType::DT_UInt16)
        {
            intermediateDataType = DataType::DT_Int16;
        }
    }

    frames.resize(numFrames);

    for(int frame=0; frame < numFrames; ++frame)  
    {
        const DiPixel* pixels = image.getInterData();
        if (!pixels)
        {
            RAISE_RUNTIME_ERROR << "DCMImageDriver: cannot extract pixel data fro file " << m_filePath;
        }
        if (numFramePixels != pixels->getCount())
        {
            RAISE_RUNTIME_ERROR << "DCMImageDriver: Unexpected number of pixels received for a frame. Expected:"
                << numFramePixels << ". Received: " << pixels->getCount() << ". File:" << m_filePath;
        }
        const auto* frameDataPtr = static_cast<const uint8_t*>(pixels->getData());

        cv::Mat cvImage(image.getWidth(), image.getHeight(), CV_MAKE_TYPE(cvIntermediateType, numChannels),
            (void*)frameDataPtr);

        if (intermediateDataType == originalDataType && !m_useRescaling)
        {
            cvImage.copyTo(frames[frame]);
        }
        else
        {
            cv::Mat cvImageTmp;
            cvImage.convertTo(cvImageTmp, CV_MAKE_TYPE(cvOriginalType, numChannels), m_rescaleSlope, -m_rescaleIntercept);
            cvImageTmp.copyTo(frames[frame]);
        }
        image.processNextFrames();
    }
}


DcmDataset* DCMFile::getDataset() const
{
    return m_file ? m_file->getDataset() : nullptr;
}

DcmDataset* DCMFile::getValidDataset() const
{
    if (!m_file)
    {
        RAISE_RUNTIME_ERROR << "DCMImageDriver: uninitialized DICOM file:" << m_filePath;
    }
    DcmDataset* dataset = m_file->getDataset();
    if (!dataset)
    {
        RAISE_RUNTIME_ERROR << "DCMImageDriver: cannot retrieve DICOM dataset from file:" << m_filePath;
    }
    return dataset;
}

bool DCMFile::getIntTag(const DcmTagKey& tag, int& value, int pos) const
{
    bool ok(false);
    DcmElement* element = nullptr;
    DcmDataset* dataset = getValidDataset();

    OFCondition cond = dataset->findAndGetElement(tag, element);
    if (cond != EC_Normal)
    {
        element = nullptr;
    }

    if (nullptr == element)
    {
        value = -1;
        return false;
    }

    Uint16 valueU16;
    if (EC_Normal == element->getUint16(valueU16, pos))
    {
        value = valueU16;
        return true;
    }

    Sint16 valueS16;
    if (EC_Normal == element->getSint16(valueS16, pos))
    {
        value = valueS16;
        return true;
    }

    Uint32 valueU32;
    if (EC_Normal == element->getUint32(valueU32, pos))
    {
        value = valueU32;
        return true;
    }

    Sint32 valueS32;
    if (EC_Normal == element->getSint32(valueS32, pos))
    {
        value = valueS32;
        return true;
    }

    value = -1;
    return false;
}

bool DCMFile::getDblTag(const DcmTagKey& tag, double& value, double defaultValue)
{
    bool ok(false);
    DcmElement* element = nullptr;
    DcmDataset* dataset = getValidDataset();

    OFCondition cond = dataset->findAndGetElement(tag, element);
    if (cond != EC_Normal)
    {
        element = nullptr;
    }

    if (nullptr == element)
    {
        value = -1;
        return false;
    }
    if (dataset->findAndGetFloat64(tag, value).good())
    {
        ok = true;
    }
    else
    {
        value = defaultValue;
    }
    return ok;
}

bool DCMFile::getStringTag(const DcmTagKey& tag, std::string& value) const
{
    bool ok(false);
    bool bRet(false);
    DcmDataset* dataset = getValidDataset();
    OFString dstrVal;
    if (dataset->findAndGetOFString(tag, dstrVal).good())
    {
        value = dstrVal.c_str();
        ok = true;
    }
    return ok;
}
