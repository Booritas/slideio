// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "jp2codecparameter.hpp"
using namespace slideio;

//--------------------------------------------------------------------------------
//  Method: constructor
//  Parameters: 
//              OFBool pVerbose:
//              OFBool pCreateSOPInstanceUID:
//              OFBool pReverseDecompressionByteOrder:
//  Return value:
//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
Jp2CodecParameter::Jp2CodecParameter(OFBool pVerbose, OFBool pCreateSOPInstanceUID, OFBool pReverseDecompressionByteOrder)
{
	createInstanceUID = pCreateSOPInstanceUID;
	reverseDecompressionByteOrder = pReverseDecompressionByteOrder;
	verboseMode = pVerbose;
}

//--------------------------------------------------------------------------------
//  Method: const
//  Parameters: 
//              const Jp2CodecParameter& src:
//  Return value:
//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
Jp2CodecParameter::Jp2CodecParameter(const Jp2CodecParameter& src)
{
	createInstanceUID = src.createInstanceUID;
	reverseDecompressionByteOrder = src.reverseDecompressionByteOrder;
	verboseMode = src.verboseMode;
}

//--------------------------------------------------------------------------------
//  Method: destructor
//  Parameters: 
//  Return value:
//--------------------------------------------------------------------------------
// destructor
//--------------------------------------------------------------------------------
Jp2CodecParameter::~Jp2CodecParameter(void)
{
}

//--------------------------------------------------------------------------------
//  Method: clone
//  Parameters: 
//  Return value:
//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
DcmCodecParameter* Jp2CodecParameter::clone() const
{
	return new Jp2CodecParameter(*this);
}

//--------------------------------------------------------------------------------
//  Method: char
//  Parameters: 
//  Return value:
//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
const char* Jp2CodecParameter::className() const
{
	return "Jp2CodecParameter";
}
