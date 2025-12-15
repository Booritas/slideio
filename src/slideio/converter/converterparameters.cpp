// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "converterparameters.hpp"

#include "convertertools.hpp"
#include "slideio/base/exceptions.hpp"

using namespace slideio;
using namespace slideio::converter;


ConverterParameters::ConverterParameters(ImageFormat format, Container containerType, slideio::Compression compression) {
    initialize();
    m_format = format;
    if (containerType == TIFF_CONTAINER) {
        m_containerParameters = std::make_shared<TIFFContainerParameters>();
	} else {
		RAISE_RUNTIME_ERROR << "ConverterParameters: Unsupported container type " << static_cast<int>(containerType);
	}
    if (compression == Compression::Jpeg) {
        m_encodeParameters = std::make_shared<JpegEncodeParameters>();

    } else if (compression == Compression::Jpeg2000) {
        m_encodeParameters =  std::make_shared<JP2KEncodeParameters>();
    }
    else {
        RAISE_RUNTIME_ERROR << "ConverterParameters: Unsupported compression type " << static_cast<int>(compression);
    }
}

ConverterParameters::ConverterParameters(const ConverterParameters& other) {
    copyFrom(other);
}

ConverterParameters& ConverterParameters::operator=(const ConverterParameters& other) {
    if (this != &other) {
        copyFrom(other);
    }
    return *this;
}

void ConverterParameters::copyFrom(const ConverterParameters& other) {
    m_format = other.m_format;
    m_rect = other.m_rect;
    m_channelRange = other.m_channelRange;
    m_sliceRange = other.m_sliceRange;
    m_frameRange = other.m_frameRange;

    // Deep copy encode parameters
    if (other.m_encodeParameters) {
        Compression compression = other.m_encodeParameters->getCompression();
        if (compression == Compression::Jpeg) {
            auto jpegParams = std::static_pointer_cast<JpegEncodeParameters>(other.m_encodeParameters);
            m_encodeParameters = std::make_shared<JpegEncodeParameters>(jpegParams->getQuality());
        } else if (compression == Compression::Jpeg2000) {
            auto jp2kParams = std::static_pointer_cast<JP2KEncodeParameters>(other.m_encodeParameters);
            auto newParams = std::make_shared<JP2KEncodeParameters>(
                jp2kParams->getCompressionRate(), 
                jp2kParams->getCodecFormat()
            );
            newParams->setSubSamplingDx(jp2kParams->getSubSamplingDx());
            newParams->setSubSamplingDy(jp2kParams->getSubSamplingDy());
            m_encodeParameters = newParams;
        } else {
            m_encodeParameters = nullptr;
        }
    } else {
        m_encodeParameters = nullptr;
    }

    // Deep copy container parameters
    if (other.m_containerParameters) {
        Container containerType = other.m_containerParameters->getContainerType();
        if (containerType == TIFF_CONTAINER) {
            auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(other.m_containerParameters);
            auto newParams = std::make_shared<TIFFContainerParameters>();
            newParams->setTileWidth(tiffParams->getTileWidth());
            newParams->setTileHeight(tiffParams->getTileHeight());
            newParams->setNumZoomLevels(tiffParams->getNumZoomLevels());
            m_containerParameters = newParams;
        } else {
            m_containerParameters = nullptr;
        }
    } else {
        m_containerParameters = nullptr;
    }
}

void ConverterParameters::updateNotDefinedParameters(const std::shared_ptr<CVScene>& scene) {
    if (!m_rect.valid()) {
        cv::Rect rect = scene->getRect();
        m_rect = Rect(rect.x, rect.y, rect.width, rect.height);
    }
    if (m_channelRange.size() <= 0) {
        m_channelRange = cv::Range(0, scene->getNumChannels());
    }
    if (m_sliceRange.size() <= 0) {
        if (m_format == ImageFormat::SVS) {
			m_sliceRange = cv::Range(0, 1);
        } else if (m_format == ImageFormat::OME_TIFF) {
			m_sliceRange = cv::Range(0, scene->getNumZSlices());
        }
    }
    if (m_frameRange.size() <= 0) {
        if (m_format == ImageFormat::SVS) {
            m_frameRange = cv::Range(0, 1);
        }
        else if (m_format == ImageFormat::OME_TIFF) {
            m_frameRange = cv::Range(0, scene->getNumTFrames());
        }
    }
    if (m_containerParameters != nullptr) {
        if (m_containerParameters->getContainerType() == TIFF_CONTAINER) {
            auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(m_containerParameters);
            if (tiffParams->getNumZoomLevels() < 1) {
                int numZoomLevels = ConverterTools::computeNumZoomLevels(m_rect.width, m_rect.height);
                tiffParams->setNumZoomLevels(numZoomLevels);
            }
		}
    }
}

void ConverterParameters::initialize() {
    m_format = ImageFormat::Unknown;
    m_rect = Rect(0, 0, 0, 0);
    m_channelRange = cv::Range(0, 0);
    m_sliceRange = cv::Range(0, 0);
    m_frameRange = cv::Range(0, 0);
}

Compression ConverterParameters::getEncoding() const {
    if (m_encodeParameters == nullptr) {
        RAISE_RUNTIME_ERROR << "Converter: Image encoding parameters are not defined!";
    }
    return m_encodeParameters->getCompression();
}

Container ConverterParameters::getContainerType() const {
    if (m_containerParameters == nullptr) {
        RAISE_RUNTIME_ERROR << "Converter: Image container parameters are not defined!";
    }
    return m_containerParameters->getContainerType();
}
