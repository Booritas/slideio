// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "dcmtk/dcmdata/dctypes.h"

namespace slideio
{
    class Jp2CodecParameter;
    class Jp2Decoder;

    class Jp2DecoderRegistration
    {
    public:
        static void registerCodecs(
            OFBool pCreateSOPInstanceUID = OFFalse,
            OFBool pVerbose = OFFalse,
            OFBool pReverseDecompressionByteOrder = OFFalse);
        static void cleanup();
    private:
        Jp2DecoderRegistration(const Jp2DecoderRegistration&);
        Jp2DecoderRegistration& operator=(const Jp2DecoderRegistration&);
        static OFBool registered;
        static Jp2CodecParameter* cp;
        static Jp2Decoder* codec;
    };
    
}
