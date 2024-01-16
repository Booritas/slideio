// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "dcmtk/dcmdata/dccodec.h"    /* for class DcmCodec */
//----------------------------------------------
// Jp2Decoder class description
//----------------------------------------------
namespace slideio
{

    class Jp2Decoder : public DcmCodec
    {
    public:
        enum Photometric
        {
            DVPI_Unknown,
            DVPI_Monochrome1,
            DVPI_Monochrome2,
            DVPI_PaletteColor,
            DVPI_RGB,
            DVPI_HSV,
            DVPI_ARGB,
            DVPI_CMYK,
            DVPI_YBR_Full,
            DVPI_YBR_Full_422,
            DVPI_YBR_Partial_422,
            DVPI_YBR_RCT,
            DVPI_YBR_ICT,
        };

        static Photometric DVPhotometricFromDCMTKString(const char* szName);
        //----------------------------------------------
        // construction
        //----------------------------------------------
    public:
        //! constructor
        Jp2Decoder(void);
        //! destructor
        ~Jp2Decoder(void);
        /** decompresses the given pixel sequence and
     *  stores the result in the given uncompressedPixelData element.
     *  @param fromRepParam current representation parameter of compressed data, may be NULL
     *  @param pixSeq compressed pixel sequence
     *  @param uncompressedPixelData uncompressed pixel data stored in this element
     *  @param cp codec parameters for this codec
     *  @param objStack stack pointing to the location of the pixel data
     *    element in the current dataset.
     *  @return EC_Normal if successful, an error code otherwise.
     */
        virtual OFCondition decode(
            const DcmRepresentationParameter* fromRepParam,
            DcmPixelSequence* pixSeq,
            DcmPolymorphOBOW& uncompressedPixelData,
            const DcmCodecParameter* cp,
            const DcmStack& objStack) const;

        /** compresses the given uncompressed DICOM image and stores
     *  the result in the given pixSeq element.
     *  @param pixelData pointer to the uncompressed image data in OW format
     *    and local byte order
     *  @param length of the pixel data field in bytes
     *  @param toRepParam representation parameter describing the desired
     *    compressed representation (e.g. JPEG quality)
     *  @param pixSeq compressed pixel sequence (pointer to new DcmPixelSequence object
     *    allocated on heap) returned in this parameter upon success.
     *  @param cp codec parameters for this codec
     *  @param objStack stack pointing to the location of the pixel data
     *    element in the current dataset.
     *  @return EC_Normal if successful, an error code otherwise.
     */
        virtual OFCondition encode(
            const Uint16* pixelData,
            const Uint32 length,
            const DcmRepresentationParameter* toRepParam,
            DcmPixelSequence*& pixSeq,
            const DcmCodecParameter* cp,
            DcmStack& objStack) const;

        /** transcodes (re-compresses) the given compressed DICOM image and stores
     *  the result in the given toPixSeq element.
     *  @param fromRepType current transfer syntax of the compressed image
     *  @param fromRepParam current representation parameter of compressed data, may be NULL
     *  @param fromPixSeq compressed pixel sequence
     *  @param toRepParam representation parameter describing the desired
     *    new compressed representation (e.g. JPEG quality)
     *  @param toPixSeq compressed pixel sequence (pointer to new DcmPixelSequence object
     *    allocated on heap) returned in this parameter upon success.
     *  @param cp codec parameters for this codec
     *  @param objStack stack pointing to the location of the pixel data
     *    element in the current dataset.
     *  @return EC_Normal if successful, an error code otherwise.
     */
        virtual OFCondition encode(
            const E_TransferSyntax fromRepType,
            const DcmRepresentationParameter* fromRepParam,
            DcmPixelSequence* fromPixSeq,
            const DcmRepresentationParameter* toRepParam,
            DcmPixelSequence*& toPixSeq,
            const DcmCodecParameter* cp,
            DcmStack& objStack) const;

        /** checks if this codec is able to convert from the
     *  given current transfer syntax to the given new
     *  transfer syntax
     *  @param oldRepType current transfer syntax
     *  @param newRepType desired new transfer syntax
     *  @return true if transformation is supported by this codec, false otherwise.
     */
        virtual OFBool canChangeCoding(
            const E_TransferSyntax oldRepType,
            const E_TransferSyntax newRepType) const;

        OFCondition decode(const DcmRepresentationParameter* fromRepParam, DcmPixelSequence* pixSeq,
            DcmPolymorphOBOW& uncompressedPixelData, const DcmCodecParameter* cp, const DcmStack& objStack,
            OFBool& removeOldRep) const override;
        OFCondition decodeFrame(const DcmRepresentationParameter* fromParam, DcmPixelSequence* fromPixSeq,
            const DcmCodecParameter* cp, DcmItem* dataset, Uint32 frameNo, Uint32& startFragment, void* buffer,
            Uint32 bufSize, std::string& decompressedColorModel) const override;
        OFCondition encode(const Uint16* pixelData, const Uint32 length, const DcmRepresentationParameter* toRepParam,
            DcmPixelSequence*& pixSeq, const DcmCodecParameter* cp, DcmStack& objStack,
            OFBool& removeOldRep) const override;
        OFCondition encode(const E_TransferSyntax fromRepType, const DcmRepresentationParameter* fromRepParam,
            DcmPixelSequence* fromPixSeq, const DcmRepresentationParameter* toRepParam, DcmPixelSequence*& toPixSeq,
            const DcmCodecParameter* cp, DcmStack& objStack, OFBool& removeOldRep) const override;
        OFCondition determineDecompressedColorModel(const DcmRepresentationParameter* fromParam,
            DcmPixelSequence* fromPixSeq, const DcmCodecParameter* cp, DcmItem* dataset,
            std::string& decompressedColorModel) const override;
    };
    
}
