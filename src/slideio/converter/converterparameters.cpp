// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "converterparameters.hpp"

#include "slideio/base/exceptions.hpp"

using namespace slideio;
using namespace slideio::converter;


ConverterParameters::ConverterParameters(ImageFormat format, Container containerType, slideio::Compression compression): 
    m_format(format),
    m_zSlice(0),
    m_tFrame(0) {
    // Container
    if (containerType == TIFF_CONTAINER) {
        m_containerParameters = std::make_shared<TIFFContainerParameters>();
	} else {
		RAISE_RUNTIME_ERROR << "ConverterParameters: Unsupported container type " << static_cast<int>(containerType);
	}
	// Encoding
    if (compression == Compression::Jpeg) {
        m_encodeParameters = std::make_shared<JpegEncodeParameters>();

    } else if (compression == Compression::Jpeg2000) {
        m_encodeParameters =  std::make_shared<JP2KEncodeParameters>();
    }
    else {
        RAISE_RUNTIME_ERROR << "ConverterParameters: Unsupported compression type " << static_cast<int>(compression);
    }
}
