// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dccodec.h" /* for DcmCodecParameter */
//----------------------------------------------
// class Jp2CodecParameter description
//----------------------------------------------
namespace slideio
{
    class Jp2CodecParameter : public DcmCodecParameter
    {
    public:
        //! constructor
        Jp2CodecParameter(OFBool pVerbose = OFFalse, OFBool pCreateSOPInstanceUID = OFFalse, OFBool pReverseDecompressionByteOrder = OFFalse);
        //! copy constructor
        Jp2CodecParameter(const Jp2CodecParameter&);
        //! destructor
        ~Jp2CodecParameter(void);
        /** this methods creates a copy of type DcmCodecParameter *
	 *  it must be overweritten in every subclass.
	 *  @return copy of this object
	 */
        virtual DcmCodecParameter* clone() const;
        /** returns the class name as string.
	 *  can be used as poor man's RTTI replacement.
	 */
        virtual const char* className() const;
        /** returns mode for SOP Instance UID creation
	*  @return mode for SOP Instance UID creation
	*/
        OFBool getUIDCreation() const
        {
            return createInstanceUID;
        }

        /** returns verbose mode flag
	*  @return verbose mode flag
	*/
        OFBool isVerbose() const
        {
            return verboseMode;
        }
        /** returns reverse decompression byte order mode
   *  @return reverse decompression byte order mode
   */
        OFBool getReverseDecompressionByteOrder() const
        {
            return reverseDecompressionByteOrder;
        }
    protected:
        /// create new Instance UID during compression/decompression?
        OFBool createInstanceUID;
        /** enable reverse byte order of RLE segments during decompression, needed to
	*  decompress certain incorrectly encoded RLE images
	*/
        OFBool reverseDecompressionByteOrder;
        /// verbose mode flag. If true, warning messages are printed to console
        OFBool verboseMode;
    };
    
}
