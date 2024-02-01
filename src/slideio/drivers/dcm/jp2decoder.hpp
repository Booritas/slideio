// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "dcmtk/dcmdata/dccodec.h"

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
    public:
        Jp2Decoder(void);
        ~Jp2Decoder(void);

        virtual OFCondition decode(
            const DcmRepresentationParameter* fromRepParam,
            DcmPixelSequence* pixSeq,
            DcmPolymorphOBOW& uncompressedPixelData,
            const DcmCodecParameter* cp,
            const DcmStack& objStack) const;

        OFCondition encode(const Uint16* pixelData, const Uint32 length, const DcmRepresentationParameter* toRepParam,
            DcmPixelSequence*& pixSeq, const DcmCodecParameter* cp, DcmStack& objStack,
            OFBool& removeOldRep) const override;

        OFCondition encode(const E_TransferSyntax fromRepType, const DcmRepresentationParameter* fromRepParam,
            DcmPixelSequence* fromPixSeq, const DcmRepresentationParameter* toRepParam, DcmPixelSequence*& toPixSeq,
            const DcmCodecParameter* cp, DcmStack& objStack, OFBool& removeOldRep) const override;

        virtual OFBool canChangeCoding(
            const E_TransferSyntax oldRepType,
            const E_TransferSyntax newRepType) const override;

        OFCondition decode(const DcmRepresentationParameter* fromRepParam, DcmPixelSequence* pixSeq,
            DcmPolymorphOBOW& uncompressedPixelData, const DcmCodecParameter* cp, const DcmStack& objStack,
            OFBool& removeOldRep) const override;

        OFCondition decodeFrame(const DcmRepresentationParameter* fromParam, DcmPixelSequence* fromPixSeq,
            const DcmCodecParameter* cp, DcmItem* dataset, Uint32 frameNo, Uint32& startFragment, void* buffer,
            Uint32 bufSize, std::string& decompressedColorModel) const override;

        OFCondition determineDecompressedColorModel(const DcmRepresentationParameter* fromParam,
            DcmPixelSequence* fromPixSeq, const DcmCodecParameter* cp, DcmItem* dataset,
            std::string& decompressedColorModel) const override;
    };
    
}
