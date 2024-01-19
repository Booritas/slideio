// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "jp2codecparameter.hpp"
using namespace slideio;

Jp2CodecParameter::Jp2CodecParameter(OFBool pVerbose, OFBool pCreateSOPInstanceUID, OFBool pReverseDecompressionByteOrder)
{
	createInstanceUID = pCreateSOPInstanceUID;
	reverseDecompressionByteOrder = pReverseDecompressionByteOrder;
	verboseMode = pVerbose;
}

Jp2CodecParameter::Jp2CodecParameter(const Jp2CodecParameter& src)
{
	createInstanceUID = src.createInstanceUID;
	reverseDecompressionByteOrder = src.reverseDecompressionByteOrder;
	verboseMode = src.verboseMode;
}

Jp2CodecParameter::~Jp2CodecParameter(void)
{
}

DcmCodecParameter* Jp2CodecParameter::clone() const
{
	return new Jp2CodecParameter(*this);
}

const char* Jp2CodecParameter::className() const
{
	return "Jp2CodecParameter";
}
