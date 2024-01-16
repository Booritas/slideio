// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "dcmtk/dcmdata/dctypes.h"
//----------------------------------------------
// Jp2DecoderRegistration class description
//----------------------------------------------
namespace slideio
{
    class Jp2CodecParameter;
    class Jp2Decoder;

    class Jp2DecoderRegistration
    {
    public:
        /** registers JPEG 2000 decoder.
     *  If already registered, call is ignored unless cleanup() has
     *  been performed before.
     *  @param pCreateSOPInstanceUID flag indicating whether or not
     *    a new SOP Instance UID should be assigned upon decompression.
     *  @param pVerbose verbose mode flag
     *  @param pReverseDecompressionByteOrder flag indicating whether the byte order should
     *    be reversed upon decompression. Needed to correctly decode some incorrectly encoded
     *    images with more than one byte per sample.
     */
        static void registerCodecs(
            OFBool pCreateSOPInstanceUID = OFFalse,
            OFBool pVerbose = OFFalse,
            OFBool pReverseDecompressionByteOrder = OFFalse);
        /** deregisters decoder.
     *  Attention: Must not be called while other threads might still use
     *  the registered codecs, e.g. because they are currently decoding
     *  DICOM data sets through dcmdata.
     */
        static void cleanup();
    private:
        /// private undefined copy constructor
        Jp2DecoderRegistration(const Jp2DecoderRegistration&);
        /// private undefined copy assignment operator
        Jp2DecoderRegistration& operator=(const Jp2DecoderRegistration&);
        /// flag indicating whether the decoder is already registered.
        static OFBool registered;
        /// pointer to codec parameter
        static Jp2CodecParameter* cp;
        /// pointer to RLE decoder
        static Jp2Decoder* codec;
    };
    
}
