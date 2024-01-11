// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/dcm/dcmfile.hpp"
#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmimgle/dcmimage.h>
#include <boost/format.hpp>
#include "slideio/base/base.hpp"
#include "slideio/imagetools/cvtools.hpp"
#include <dcmtk/dcmdata/dcjson.h>
#include <dcmtk/dcmdata/dcpxitem.h>

#include <ostream>

#include "slideio/base/slideio_enums.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/core/tools/tools.hpp"

using namespace slideio;

class JsonFormat : public DcmJsonFormatCompact
{
public:
    bool asBulkDataURI(const DcmTagKey& tag, OFString& uri) override
    {
        if (tag == DCM_PixelData ||
            tag == DCM_RETIRED_GrayLookupTableData ||
            tag == DCM_RedPaletteColorLookupTableData ||
            tag == DCM_GreenPaletteColorLookupTableData ||
            tag == DCM_BluePaletteColorLookupTableData ||
            tag == DCM_AlphaPaletteColorLookupTableData ||
            tag == DCM_RETIRED_LargeRedPaletteColorLookupTableData ||
            tag == DCM_RETIRED_LargeGreenPaletteColorLookupTableData ||
            tag == DCM_RETIRED_LargeBluePaletteColorLookupTableData ||
            tag == DCM_RETIRED_LargePaletteColorLookupTableUID ||
            tag == DCM_SegmentedRedPaletteColorLookupTableData ||
            tag == DCM_SegmentedGreenPaletteColorLookupTableData ||
            tag == DCM_SegmentedBluePaletteColorLookupTableData ||
            tag == DCM_SegmentedAlphaPaletteColorLookupTableData)
        {
            return OFTrue;
        }
        return OFFalse;
    }
};

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

void DCMFile::loadFile()
{
#if defined(WIN32)
    std::wstring filePathW = Tools::toWstring(m_filePath);
    OFFilename filename(filePathW.c_str());
    OFCondition status = m_file->loadFile(filename);
#else
    OFCondition status = m_file->loadFile(m_filePath.c_str());
#endif
    if (status.bad())
    {
        RAISE_RUNTIME_ERROR << "DCMImageDriver: Cannot open file: " << m_filePath;
    }
}

std::shared_ptr<DicomImage> DCMFile::createImage(int firstFrame, int numFrames)
{
    DcmDataset* dataset = getDataset();
    std::shared_ptr<DicomImage> image;
    if (!dataset)
    {
        RAISE_RUNTIME_ERROR << "DCMImageDriver: unexpected null as dataset for file " << m_filePath;
    }
    E_TransferSyntax xfer = dataset->getOriginalXfer();
    image.reset(new DicomImage(dataset, xfer, CIF_UsePartialAccessToPixelData, (ulong)firstFrame, (ulong)numFrames));
    if (image->getStatus() != EIS_Normal)
    {
        RAISE_RUNTIME_ERROR << "DCMImageDriver: cannot decompress file " << m_filePath << ". Image status: "
            << image->getStatus();
    }
    return image;
}


void DCMFile::init()
{
    SLIDEIO_LOG(INFO) << "DCMFile::init: initializing DICOM file " << m_filePath;

    loadFile();
    std::shared_ptr<DicomImage> image;
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
    if (!getStringTag(DCM_Modality, m_modality)) {
        m_modality = "Unknown modality";
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

    getDblTag(DCM_RescaleSlope, m_rescaleSlope, 1.);
    getDblTag(DCM_RescaleIntercept, m_rescaleIntercept, 0.);
    m_useRescaling = std::abs(m_rescaleSlope - 1.) > 1.e-6 || m_rescaleIntercept > 0.9;

    getStringTag(DCM_SeriesDescription, m_seriesDescription);
    if (!getIntTag(DCM_BitsAllocated, m_bitsAllocated))
    {
        RAISE_RUNTIME_ERROR << "DCMImageDriver: undefined valude for DCM_BitsAllocated tag. File:" << m_filePath;
    }

    int pixelRepresentation(0);
    if (!getIntTag(DCM_PixelRepresentation, pixelRepresentation))
    {
        RAISE_RUNTIME_ERROR << "DCMImageDriver: undefined value for DCM_PixelRepresentation tag. File:" << m_filePath;
    }
    const int PXREP_SIGNED = 1;
    const int PXREP_UNSIGNED = 0;
    const int bits = image.get()?image->getDepth(): m_bitsAllocated;

    if (bits == 8)
    {
        m_dataType = pixelRepresentation == PXREP_SIGNED ? DataType::DT_Int8 : DataType::DT_Byte;
    }
    else if (bits > 8)
    {
        m_dataType = pixelRepresentation == PXREP_SIGNED ? DataType::DT_Int16 : DataType::DT_UInt16;
    }
    else
    {
        RAISE_RUNTIME_ERROR << "DCMImageDriver: unexpected value for allocated bits: "
            << bits << "(" << m_bitsAllocated << ")";
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
    if (m_photoInterpretation == EPhotoInterpetation::PHIN_PALETTE)
    {
        m_numChannels = 3;
    }

    std::string sopClassUID;
    getStringTag(DCM_SOPClassUID, sopClassUID);
    std::string dimensionOrganization;
    getStringTag(DCM_DimensionOrganizationType, dimensionOrganization);

    m_WSISlide = (sopClassUID == UID_VLWholeSlideMicroscopyImageStorage);
    if(m_WSISlide && dimensionOrganization != "TILED_FULL") {
        RAISE_RUNTIME_ERROR << "DCMImageDriver: Unsupported dimension organization type for WSI file. Expected: TILED_FULL, received:" << dimensionOrganization;
    }

    if(m_WSISlide) {
        m_frames = m_slices;
        m_slices = 1;
        m_tileSize = { m_width, m_height };
        m_width = m_height = 0;
        if(!getIntTag(DCM_TotalPixelMatrixColumns, m_width)) {
            RAISE_RUNTIME_ERROR << "DCMImageDriver: Cannot extract total pixel matrix columns for WSI file:" << m_filePath;
        }
        if (!getIntTag(DCM_TotalPixelMatrixRows, m_height)) {
            RAISE_RUNTIME_ERROR << "DCMImageDriver: Cannot extract total pixel matrix rows for WSI file:" << m_filePath;
        }
    } else {
        try {
            image = createImage();
        }
        catch (slideio::RuntimeError& err) {
            m_decompressWholeFile = true;
            SLIDEIO_LOG(WARNING) << "DCMFile::init: Cannot create DicomImage instance for the file:"
                << m_filePath
                << ". Trying to decomress the whole file. Error message:"
                << err.what();
            OFCondition cond = dataset->chooseRepresentation(EXS_LittleEndianExplicit, nullptr);
            if (!cond.good()) {
                RAISE_RUNTIME_ERROR << "DCMFile::init Cannot decompress the file "
                    << m_filePath
                    << ". Error message:"
                    << cond.text();
            }
        }
    }
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
        SLIDEIO_LOG(WARNING) << "DCMImageDriver: Unknown xTransfer:" << xfer << " for file " << m_filePath;
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
    SLIDEIO_LOG(INFO) << "DICOM file: " << m_filePath << std::endl
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

inline int getPixelRepresentationDataSize(EP_Representation rep)
{
    switch (rep)
    {
    case EPR_Uint8:
    case EPR_Sint8:
        return 1;
    case EPR_Uint16:
    case EPR_Sint16:
        return 2;
    case EPR_Uint32:
    case EPR_Sint32:
        return 4;
    }
    RAISE_RUNTIME_ERROR << "DCMImageDriver: unexpected pixel representation:" << (int)rep;
}

inline int getCvTypeForPixelRepresentation(EP_Representation rep)
{
    switch (rep)
    {
    case EPR_Uint8:
        return CV_8U;
    case EPR_Sint8:
        return CV_8S;
    case EPR_Uint16:
        return CV_16U;
    case EPR_Sint16:
        return CV_16S;
    case EPR_Uint32:
        return CV_32S;
    case EPR_Sint32:
        return CV_32S;
    }
    RAISE_RUNTIME_ERROR << "DCMImageDriver: unexpected pixel representation:" << (int)rep;
}

void DCMFile::extractPixelsPartialy(std::vector<cv::Mat>& frames, int startFrame, int numFrames)
{
    SLIDEIO_LOG(INFO) << "Extracting pixel values with partial decompression.";

    DcmDataset* dataset = getDataset();
    if (!dataset)
    {
        RAISE_RUNTIME_ERROR << "DCMImageDriver: unexpected null as dataset for file " << m_filePath;
    }
    E_TransferSyntax xfer = dataset->getOriginalXfer();
    std::shared_ptr<DicomImage> image = createImage((ulong)startFrame, (ulong)1);

    const int numFileFrames = image->getFrameCount();
    const int numChannels = getNumChannels();
    const DataType originalDataType = getDataType();
    const int numFramePixels = isWSIFile()?(m_tileSize.width*m_tileSize.height):(getWidth() * getHeight());
    const int cvOriginalType = CVTools::toOpencvType(originalDataType);

    frames.resize(numFrames);
    const int bits = image->getDepth();
    for (int frame = 0; frame < numFrames; ++frame)
    {
        const DiPixel* pixels = image->getInterData();
        EP_Representation rep = pixels->getRepresentation();
        int cvIntermediateType = getCvTypeForPixelRepresentation(rep);
        const int numFrameBytes = numChannels * numFramePixels * getPixelRepresentationDataSize(rep);

        if (m_useRescaling)
        {
            if (cvIntermediateType == CV_16U)
            {
                cvIntermediateType = CV_16S;
            }
        }

        if (!pixels)
        {
            RAISE_RUNTIME_ERROR << "DCMImageDriver: cannot extract pixel data fro file " << m_filePath;
        }
        if (numFramePixels != pixels->getCount())
        {
            RAISE_RUNTIME_ERROR << "DCMImageDriver: Unexpected number of pixels received for a frame. Expected:"
                << numFramePixels << ". Received: " << pixels->getCount() << ". File:" << m_filePath;
        }
        if (numChannels != pixels->getPlanes())
        {
            RAISE_RUNTIME_ERROR << "DCMImageDriver: Unexpected number of planes received for a frame. Expected:"
                << numChannels << ". Received: " << pixels->getPlanes() << ". File:" << m_filePath;
        }
        const auto* frameDataPtr = static_cast<const uint8_t*>(pixels->getData());
        if (numChannels == 1)
        {
            frames[frame].create(image->getHeight(), image->getWidth(), CV_MAKE_TYPE(cvIntermediateType, numChannels));
            std::memcpy(frames[frame].data, frameDataPtr, numFrameBytes);
            if (cvIntermediateType != cvOriginalType || m_useRescaling)
            {
                frames[frame].convertTo(frames[frame], CV_MAKE_TYPE(cvOriginalType, numChannels), m_rescaleSlope,
                                        -m_rescaleIntercept);
            }
        }
        else if (numChannels == 3)
        {
            void** channels = (void**)frameDataPtr;
            void* red = channels[0];
            void* green = channels[1];
            void* blue = channels[2];

            cv::Mat channelR(image->getHeight(), image->getWidth(), CV_MAKE_TYPE(cvIntermediateType, 1), red);
            cv::Mat channelG(image->getHeight(), image->getWidth(), CV_MAKE_TYPE(cvIntermediateType, 1), green);
            cv::Mat channelB(image->getHeight(), image->getWidth(), CV_MAKE_TYPE(cvIntermediateType, 1), blue);
            std::vector<cv::Mat> rgb = {channelR, channelG, channelB};
            cv::merge(rgb, frames[frame]);
            if (cvIntermediateType != cvOriginalType || m_useRescaling)
            {
                frames[frame].convertTo(frames[frame], CV_MAKE_TYPE(cvOriginalType, numChannels), m_rescaleSlope,
                    -m_rescaleIntercept);
            }
        }
        else
        {
            RAISE_RUNTIME_ERROR <<
                "DCMImageDriver: Unexpected number of planes received for a frame. Accepted values: 1 or 3."
                << " Received: " << pixels->getPlanes() << ". File:" << m_filePath;
        }

        image->processNextFrames();
    }
}

void DCMFile::extractPixelsWholeFileDecompression(std::vector<cv::Mat>& frames, int startFrame, int numFrames)
{
    SLIDEIO_LOG(INFO) << "Extracting pixel values with partial decompression.";
    DcmDataset* dataset = getDataset();
    if (!dataset)
    {
        RAISE_RUNTIME_ERROR << "DCMImageDriver: unexpected null as dataset for file " << m_filePath;
    }

    DcmElement* elem = nullptr;
    OFCondition cond = dataset->findAndGetElement(DCM_PixelData, elem);
    if(!cond.good())
    {
        RAISE_RUNTIME_ERROR << "DCMImageDriver: Cannot extract pixel data. Error:"
            << cond.text();
    }
    DcmEVR evr = elem->getVR();
    uint8_t* data(nullptr);
    if (evr == EVR_OW)
    {
        cond = elem->getUint16Array((Uint16*&)data);
    }
    else // evr == EVR_OB
    {
        cond = elem->getUint8Array((Uint8*&)data);
    }

    if(!cond.good())
    {
        RAISE_RUNTIME_ERROR << "DCMImageDriver: Error by getting pixel array. Message:"
            << cond.text();
    }
    const int dataSize = elem->getLength();
    const int numFileFrames = getNumSlices();
    const int numChannels = getNumChannels();
    const DataType dt = getDataType();
    const int numChannelPixels = getWidth() * getHeight();
    const int cvType = CVTools::toOpencvType(dt);
    const int ds = CVTools::cvGetDataTypeSize(dt);
    const int numFramePixels = numChannelPixels * numChannels;
    const int numFrameBytes = ds * numFramePixels;

    std::vector<uint16_t> dataBuff;
    if(m_bitsAllocated==12 && (dt == DataType::DT_Int16 || dt == DataType::DT_UInt16) && dataSize == (numFrameBytes/4)*3)
    {
        // workaround for 12 allocated bit pixels
        dataBuff.resize(numFramePixels);
        Tools::convert12BitsTo16Bits(data, dataBuff.data(), numFramePixels);
        data = (uint8_t*)dataBuff.data();
    }
    else
    {
        if(dataSize != numFrameBytes)
        {
            RAISE_RUNTIME_ERROR << "DCMImageDriver: Unexpected size of pixel data. Expected: "
                << numFrameBytes << ". Received: " << dataSize;
        }
    }

    frames.resize(numFrames);
    const uint8_t* frameDataPtr = data + startFrame * numFrameBytes;
    for(int frame=0; frame<numFrames; ++frame, frameDataPtr += numFrameBytes)
    {
        if (numChannels == 1)
        {
            frames[frame].create(getHeight(), getWidth(), CV_MAKE_TYPE(cvType, numChannels));
            std::memcpy(frames[frame].data, frameDataPtr, numFrameBytes);
        }
        else if (numChannels == 3)
        {
            void** channels = (void**)frameDataPtr;
            void* red = channels[0];
            void* green = channels[1];
            void* blue = channels[2];

            cv::Mat channelR(getHeight(), getWidth(), CV_MAKE_TYPE(cvType, 1), red);
            cv::Mat channelG(getHeight(), getWidth(), CV_MAKE_TYPE(cvType, 1), green);
            cv::Mat channelB(getHeight(), getWidth(), CV_MAKE_TYPE(cvType, 1), blue);
            std::vector<cv::Mat> rgb = { channelR, channelG, channelB };
            cv::merge(rgb, frames[frame]);
        }
        else
        {
            RAISE_RUNTIME_ERROR <<
                "DCMImageDriver: Unexpected number of channels received for a frame. Accepted values: 1 or 3."
                << " Received: " << numChannels << ". File:" << m_filePath;
        }

    }
}

void DCMFile::readPixelValues(std::vector<cv::Mat>& frames, int startFrame, int numFrames)
{
    SLIDEIO_LOG(INFO) << "Extracting pixel values from the dataset";
    if(!m_decompressWholeFile)
    {
        extractPixelsPartialy(frames, startFrame, numFrames);
    }
    else
    {
        extractPixelsWholeFileDecompression(frames, startFrame, numFrames);
    }
}

std::string DCMFile::getMetadata()
{
    DcmDataset* dataset = getValidDataset();
    JsonFormat format;
    std::stringstream os;
    os << "{";
    const OFCondition code = dataset->writeJson(os, format);
    if (!code.good())
    {
        RAISE_RUNTIME_ERROR << "DCMImageDriver: Error by retrieving of metadata. "
            << code.text();
    }
    os << "}";
    return os.str();
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
        value = defaultValue;
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

bool DCMFile::isDicomDirFile(const std::string& filePath)
{
    bool isDicomDir = false;
    DcmFileFormat file;
    if (file.loadFile(filePath.c_str(), EXS_Unknown, EGL_noChange, DCM_MaxReadLength, ERM_metaOnly).good())
    {
        DcmMetaInfo* metainfo = file.getMetaInfo();
        if (metainfo)
        {
            OFString dstrVal;
            if (metainfo->findAndGetOFString(DCM_MediaStorageSOPClassUID, dstrVal).good())
            {
                isDicomDir = dstrVal == UID_MediaStorageDirectoryStorage;
            }
        }
    }
    return isDicomDir;
}

bool DCMFile::isWSIFile(const std::string& filePath) {
    bool isWSI = false;
    DcmFileFormat file;
    if (file.loadFile(filePath.c_str()).good())
    {
        DcmDataset* dataset = file.getDataset();
        if (dataset)
        {
            OFString sopClassUID;
            if (dataset->findAndGetOFString(DCM_SOPClassUID, sopClassUID).good())
            {
                isWSI = sopClassUID == UID_VLWholeSlideMicroscopyImageStorage;
            }
        }
    }
    return isWSI;
}

bool DCMFile::getTileRect(int tileIndex, cv::Rect& tileRect) const{
    if(!m_WSISlide) {
        RAISE_RUNTIME_ERROR << "DCMFile::getTileRect: the file " << getFilePath() << " is not a WSI slide.";
    }
    if(tileIndex < 0 || tileIndex >= m_frames) {
        RAISE_RUNTIME_ERROR << "DCMFile::getTileRect: tile index is out of range. Number of tiles: "
            << m_frames << " . Received index: " << tileIndex;
    }
    const int tilesX = (m_width - 1)/m_tileSize.width + 1;
    const int col = tileIndex % tilesX;
    const int row = tileIndex / tilesX;
    tileRect = cv::Rect(col*m_tileSize.width, row*m_tileSize.height, m_tileSize.width, m_tileSize.height);
    return true;
}

bool DCMFile::readTile(int tileIndex, cv::OutputArray tileRaster) {
    if (!m_WSISlide) {
        RAISE_RUNTIME_ERROR << "DCMFile::getTileRect: the file " << getFilePath() << " is not a WSI slide.";
    }
    DcmDataset* dataset = getValidDataset();
    if(!dataset) {
        RAISE_RUNTIME_ERROR << "DCMFile::readTile: unexpected null as dataset for file " << m_filePath;
    }
    tileRaster.create(m_tileSize, CV_MAKETYPE(CVTools::cvTypeFromDataType(m_dataType),getNumChannels()));
    cv::Mat mat = tileRaster.getMat();
    std::vector<cv::Mat> frames;
    frames.push_back(mat);
    extractPixelsPartialy(frames, tileIndex, 1);
    return true;
}

