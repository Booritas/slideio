// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//----------------------------------------------
// includes
//----------------------------------------------
#include "jp2decoder.hpp"
#include "jp2codecparameter.hpp"

#include "dcmtk/dcmdata/dcdatset.h"  /* for class DcmDataset */
#include "dcmtk/dcmdata/dcdeftag.h"  /* for tag constants */
#include "dcmtk/dcmdata/dcpixseq.h"  /* for class DcmPixelSequence */
#include "dcmtk/dcmdata/dcpxitem.h"  /* for class DcmPixelItem */
#include "dcmtk/dcmdata/dcvrpobw.h"  /* for class DcmPolymorphOBOW */
#include "dcmtk/dcmdata/dcswap.h"    /* for swapIfNecessary() */
#include "dcmtk/dcmdata/dcuid.h"     /* for dcmGenerateUniqueIdentifer()*/
#include "slideio/base/slideio_enums.hpp"

using namespace slideio;

bool JP2Decode(unsigned char* pJp2Stream, long nJp2StreamLength, DataType dt, Jp2Decoder::Photometric ePh, int nWidth, int nHeight, int nChnls, unsigned char* p_buff, long buff_size);


Jp2Decoder::Photometric Jp2Decoder::DVPhotometricFromDCMTKString(const char* szName) {
	Photometric ePh(DVPI_Unknown);
	if (szName) {
		if (strcmp(szName, "MONOCHROME1") == 0) {
			ePh = DVPI_Monochrome1;
		}
		else if (strcmp(szName, "MONOCHROME2") == 0) {
			ePh = DVPI_Monochrome2;
		}
		else if (strcmp(szName, "PALETTE COLOR") == 0) {
			ePh = DVPI_PaletteColor;
		}
		else if (strcmp(szName, "RGB") == 0) {
			ePh = DVPI_RGB;
		}
		else if (strcmp(szName, "YBR_FULL") == 0) {
			ePh = DVPI_YBR_Full;
		}
		else if (strcmp(szName, "YBR_FULL_422") == 0) {
			ePh = DVPI_YBR_Full_422;
		}
		else if (strcmp(szName, "YBR_PARTIAL_422") == 0) {
			ePh = DVPI_YBR_Partial_422;
		}
		else if (strcmp(szName, "YBR_RCT") == 0) {
			ePh = DVPI_YBR_RCT;
		}
		else if (strcmp(szName, "YBR_ICT") == 0) {
			ePh = DVPI_YBR_ICT;
		}
	}
	return ePh;
}

//--------------------------------------------------------------------------------
//  Method: constructor
//  Parameters: 
//  Return value:
//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
Jp2Decoder::Jp2Decoder(void)
{
}

//--------------------------------------------------------------------------------
//  Method: destructor
//  Parameters: 
//  Return value:
//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
Jp2Decoder::~Jp2Decoder(void)
{
}

//--------------------------------------------------------------------------------
//  Method: canChangeCoding
//  Parameters: 
//              const E_TransferSyntax oldRepType:
//              const E_TransferSyntax newRepType:
//  Return value:
//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
OFBool Jp2Decoder::canChangeCoding(const E_TransferSyntax oldRepType, const E_TransferSyntax newRepType) const
{
	E_TransferSyntax myXfer = EXS_JPEG2000;
	DcmXfer newRep(newRepType);
	OFBool ret(OFFalse);
	if (newRep.isNotEncapsulated() && (oldRepType == EXS_JPEG2000 || oldRepType == EXS_JPEG2000LosslessOnly))
		ret = OFTrue; // decompress requested
	// we don't support re-coding for now.
	return ret;
}


//--------------------------------------------------------------------------------
//  Method: encode
//  Parameters: 
//              const Uint16 *:
//              const Uint32:
//              const DcmRepresentationParameter *:
//              DcmPixelSequence * &:
//              const DcmCodecParameter *:
//              DcmStack &:
//  Return value:
//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
OFCondition Jp2Decoder::encode(
	const Uint16* /* pixelData */,
	const Uint32 /* length */,
	const DcmRepresentationParameter* /* toRepParam */,
	DcmPixelSequence*& /* pixSeq */,
	const DcmCodecParameter* /* cp */,
	DcmStack& /* objStack */) const
{
	// we are a decoder only
	return EC_IllegalCall;
}

OFCondition Jp2Decoder::decode(const DcmRepresentationParameter* fromRepParam, DcmPixelSequence* pixSeq,
    DcmPolymorphOBOW& uncompressedPixelData, const DcmCodecParameter* cp, const DcmStack& objStack,
    OFBool& removeOldRep) const {
	return EC_IllegalCall;
}

OFCondition Jp2Decoder::decodeFrame(const DcmRepresentationParameter* fromParam, DcmPixelSequence* fromPixSeq,
    const DcmCodecParameter* cp, DcmItem* dataset, Uint32 frameNo, Uint32& startFragment, void* buffer, Uint32 bufSize,
    std::string& decompressedColorModel) const {
	return EC_IllegalCall;
}

OFCondition Jp2Decoder::encode(const Uint16* pixelData, const Uint32 length,
    const DcmRepresentationParameter* toRepParam, DcmPixelSequence*& pixSeq, const DcmCodecParameter* cp,
    DcmStack& objStack, OFBool& removeOldRep) const {
	return EC_IllegalCall;
}

OFCondition Jp2Decoder::encode(const E_TransferSyntax fromRepType, const DcmRepresentationParameter* fromRepParam,
    DcmPixelSequence* fromPixSeq, const DcmRepresentationParameter* toRepParam, DcmPixelSequence*& toPixSeq,
    const DcmCodecParameter* cp, DcmStack& objStack, OFBool& removeOldRep) const {
	return EC_IllegalCall;
}

OFCondition Jp2Decoder::determineDecompressedColorModel(const DcmRepresentationParameter* fromParam,
    DcmPixelSequence* fromPixSeq, const DcmCodecParameter* cp, DcmItem* dataset,
    std::string& decompressedColorModel) const {
	return EC_IllegalCall;
}


//--------------------------------------------------------------------------------
//  Method: encode
//  Parameters: 
//              const E_TransferSyntax:
//              const DcmRepresentationParameter *:
//              DcmPixelSequence *:
//              const DcmRepresentationParameter *:
//              DcmPixelSequence * &:
//              const DcmCodecParameter *:
//              DcmStack &:
//  Return value:
//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
OFCondition Jp2Decoder::encode(
	const E_TransferSyntax /* fromRepType */,
	const DcmRepresentationParameter* /* fromRepParam */,
	DcmPixelSequence* /* fromPixSeq */,
	const DcmRepresentationParameter* /* toRepParam */,
	DcmPixelSequence*& /* toPixSeq */,
	const DcmCodecParameter* /* cp */,
	DcmStack& /* objStack */) const
{
	// we don't support re-coding for now.
	return EC_IllegalCall;
}

//--------------------------------------------------------------------------------
//  Method: decode
//  Parameters: 
//              const DcmRepresentationParameter *:
//              DcmPixelSequence * pixSeq:
//              DcmPolymorphOBOW& uncompressedPixelData:
//              const DcmCodecParameter * cp:
//              const DcmStack& objStack:
//  Return value:
//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
OFCondition Jp2Decoder::decode(const DcmRepresentationParameter* /* fromRepParam */, DcmPixelSequence* pixSeq,
	DcmPolymorphOBOW& uncompressedPixelData, const DcmCodecParameter* cp, const DcmStack& objStack) const
{
	OFCondition result = EC_Normal;

	// assume we can cast the codec parameter to what we need
	const Jp2CodecParameter* djcp = OFstatic_cast(const Jp2CodecParameter*, cp);

	OFBool enableReverseByteOrder = djcp->getReverseDecompressionByteOrder();

	DcmStack localStack(objStack);
	(void)localStack.pop();             // pop pixel data element from stack
	DcmObject* dataset = localStack.pop(); // this is the item in which the pixel data is located
	if ((!dataset) || ((dataset->ident() != EVR_dataset) && (dataset->ident() != EVR_item))) {
		result = EC_InvalidTag;
	}
	else {
		Uint16 imageSamplesPerPixel = 0;
		Uint16 imageRows = 0;
		Uint16 imageColumns = 0;
		Sint32 imageFrames = 1;
		Uint16 imageBitsAllocated = 0;
		Uint16 imageBytesAllocated = 0;
		Uint16 imagePlanarConfiguration = 0;
		DcmItem* ditem = OFstatic_cast(DcmItem*, dataset);
		OFString photometricInterpretation;
		Photometric ePh(DVPI_Unknown);
		DataType eDataType(DataType::DT_Unknown);

		if (result.good())
			result = ditem->findAndGetUint16(DCM_SamplesPerPixel, imageSamplesPerPixel);
		if (result.good())
			result = ditem->findAndGetUint16(DCM_Rows, imageRows);
		if (result.good())
			result = ditem->findAndGetUint16(DCM_Columns, imageColumns);
		if (result.good())
			result = ditem->findAndGetUint16(DCM_BitsAllocated, imageBitsAllocated);

		if (result.good()) {
			imageBytesAllocated = OFstatic_cast(Uint16, imageBitsAllocated / 8);
		}
		if ((imageBitsAllocated < 8) || (imageBitsAllocated % 8 != 0)) {
			result = EC_CannotChangeRepresentation;
		}
		if (result.good() && (imageSamplesPerPixel > 1)) {
			result = ditem->findAndGetUint16(DCM_PlanarConfiguration, imagePlanarConfiguration);
		}

		if (result.good()) {
			(void)ditem->findAndGetSint32(DCM_NumberOfFrames, imageFrames);
		}

		if (result.good()) {
			result = ditem->findAndGetOFString(DCM_PhotometricInterpretation, photometricInterpretation);
			if (result.good()) {
				ePh = DVPhotometricFromDCMTKString(photometricInterpretation.c_str());
				if ((ePh == DVPI_Unknown) && (imageSamplesPerPixel > 1)) {
					result = EC_CannotChangeRepresentation;
				}
			}
		}

		switch (imageBytesAllocated)
		{
		case 1:
			eDataType = DataType::DT_Byte;
			break;
		case 2:
			eDataType = DataType::DT_UInt16;
			break;
		case 4:
			eDataType = DataType::DT_Int32;
			break;
		default:
			result = EC_CannotChangeRepresentation;
		}

		if (imageFrames < 1)
			imageFrames = 1; // default in case this attribute contains garbage

		if (imageFrames > 1)
			result = EC_CannotChangeRepresentation; // not yet supported

		DataType dt;
		if (imageBytesAllocated == 1) {
			dt = DataType::DT_Byte;
		}
		else if (imageBytesAllocated == 2) {
			dt = DataType::DT_UInt16;
		}
		else {
			result = EC_CannotChangeRepresentation; // not yet supported
		}

		if (result.good()) {
			DcmPixelItem* pixItem = NULL;
			Uint8* jp2Data = NULL;
			const size_t bytesPerStripe = imageColumns * imageRows;

			Uint32 frameSize = imageBytesAllocated * imageRows * imageColumns * imageSamplesPerPixel;
			Uint32 totalSize = frameSize * imageFrames;
			if (totalSize & 1)
				totalSize++; // align on 16-bit word boundary
			Uint16* imageData16 = NULL;
			Sint32 currentFrame = 0;
			Uint32 currentItem = 1; // ignore offset table
			Uint32 numberOfStripes = 0;
			Uint32 fragmentLength = 0;

			result = uncompressedPixelData.createUint16Array(totalSize / sizeof(Uint16), imageData16);
			if (result.good()) {
				Uint8* imageData8 = OFreinterpret_cast(Uint8*, imageData16);
				for (currentFrame = 0; (currentFrame < imageFrames) && result.good(); currentFrame++) {
					// get first pixel item of this frame
					result = pixSeq->getItem(pixItem, currentItem);
					if (result.good()) {
						fragmentLength = pixItem->getLength();
						result = pixItem->getUint8Array(jp2Data);
						if (result.good()) {
							//if (!JP2Decode(jp2Data, fragmentLength, eDataType, ePh, imageColumns, imageRows, imageSamplesPerPixel, imageData8, frameSize)) {
							//	result = EC_CannotChangeRepresentation;
							//}
						}
					}
				}
			}
		}
		// adjust byte order for uncompressed image to little endian
		//swapIfNecessary(EBO_LittleEndian, gLocalByteOrder, imageData16, totalSize, sizeof(Uint16));
	}
	// the following operations do not affect the Image Pixel Module
	// but other modules such as SOP Common.  We only perform these
	// changes if we're on the main level of the dataset,
	// which should always identify itself as dataset, not as item.
	if (dataset->ident() == EVR_dataset) {
		// create new SOP instance UID if codec parameters require so
		if (result.good() && djcp->getUIDCreation()) {
			result = DcmCodec::newInstance(OFstatic_cast(DcmItem*, dataset), NULL, NULL, NULL);
		}
	}
	return result;
}
