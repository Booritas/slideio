// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "jp2decoderregistration.hpp"
#include "jp2codecparameter.hpp"
#include "jp2decoder.hpp"

using namespace slideio;
//----------------------------------------------
// initialization of static members
//----------------------------------------------
OFBool slideio::Jp2DecoderRegistration::registered = OFFalse;
slideio::Jp2CodecParameter* slideio::Jp2DecoderRegistration::cp = NULL;
slideio::Jp2Decoder* Jp2DecoderRegistration::codec = NULL;

//--------------------------------------------------------------------------------
//  Method: registerCodecs
//  Parameters: 
//              OFBool pCreateSOPInstanceUID:
//              OFBool pVerbose:
//              OFBool pReverseDecompressionByteOrder:
//  Return value:
//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
void Jp2DecoderRegistration::registerCodecs(OFBool pCreateSOPInstanceUID, OFBool pVerbose, OFBool pReverseDecompressionByteOrder)
{
	if (!registered) {
		cp = new Jp2CodecParameter(pVerbose, pCreateSOPInstanceUID, pReverseDecompressionByteOrder);
		if (cp) {
			codec = new Jp2Decoder();
			if (codec)
				DcmCodecList::registerCodec(codec, NULL, cp);
			registered = OFTrue;
		}
	}
}

//--------------------------------------------------------------------------------
//  Method: cleanup
//  Parameters: 
//  Return value:
//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
void Jp2DecoderRegistration::cleanup()
{
	if (registered) {
		DcmCodecList::deregisterCodec(codec);
		delete codec;
		delete cp;
		registered = OFFalse;
		codec = NULL;
		cp = NULL;
	}
}
