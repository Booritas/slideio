// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dccodec.h"

namespace slideio
{
    class Jp2CodecParameter : public DcmCodecParameter
    {
    public:
        Jp2CodecParameter(OFBool pVerbose = OFFalse, OFBool pCreateSOPInstanceUID = OFFalse, OFBool pReverseDecompressionByteOrder = OFFalse);
        Jp2CodecParameter(const Jp2CodecParameter&);
        ~Jp2CodecParameter(void);
        virtual DcmCodecParameter* clone() const;
        virtual const char* className() const;
        OFBool getUIDCreation() const
        {
            return createInstanceUID;
        }

        OFBool isVerbose() const
        {
            return verboseMode;
        }
        OFBool getReverseDecompressionByteOrder() const
        {
            return reverseDecompressionByteOrder;
        }
    protected:
        OFBool createInstanceUID;
        OFBool reverseDecompressionByteOrder;
        OFBool verboseMode;
    };
    
}
