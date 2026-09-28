// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "converterparameters.hpp"
#include "converterparametersinternal.hpp"
#include "convertertools.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/cvscene.hpp"

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
	m_tileBatchSize = other.m_tileBatchSize;

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
            newParams->setNumReadingThreads(tiffParams->getNumReadingThreads());
            newParams->setNumEncodingThreads(tiffParams->getNumEncodingThreads());
            m_containerParameters = newParams;
        } else {
            m_containerParameters = nullptr;
        }
    } else {
        m_containerParameters = nullptr;
    }
}

void slideio::converter::updateNotDefinedParameters(ConverterParameters& parameters,
                                                    const std::shared_ptr<CVScene>& scene) {
    if (!parameters.getRect().valid()) {
        cv::Rect rect = scene->getRect();
        parameters.setRect(Rect(0, 0, rect.width, rect.height));
    }
    if (parameters.getChannelRange().size() <= 0) {
        parameters.setChannelRange(Range(0, scene->getNumChannels()));
    }
    const ImageFormat format = parameters.getFormat();
    if (parameters.getSliceRange().size() <= 0) {
        if (format == ImageFormat::SVS) {
            parameters.setSliceRange(Range(0, 1));
        } else if (format == ImageFormat::OME_TIFF) {
            parameters.setSliceRange(Range(0, scene->getNumZSlices()));
        }
    }
    if (parameters.getTFrameRange().size() <= 0) {
        if (format == ImageFormat::SVS) {
            parameters.setTFrameRange(Range(0, 1));
        }
        else if (format == ImageFormat::OME_TIFF) {
            parameters.setTFrameRange(Range(0, scene->getNumTFrames()));
        }
    }
    std::shared_ptr<ContainerParameters> container = parameters.getContainerParameters();
    if (container != nullptr) {
        if (container->getContainerType() == TIFF_CONTAINER) {
            auto tiffParams = std::static_pointer_cast<TIFFContainerParameters>(container);
            if (tiffParams->getNumZoomLevels() < 1) {
                const Rect& rect = parameters.getRect();
                int numZoomLevels = ConverterTools::computeNumZoomLevels(rect.width, rect.height);
                tiffParams->setNumZoomLevels(numZoomLevels);
            }
        }
    }
}

void ConverterParameters::initialize() {
    m_format = ImageFormat::Unknown;
    m_rect = Rect(0, 0, 0, 0);
    m_channelRange = Range(0, 0);
    m_sliceRange = Range(0, 0);
    m_frameRange = Range(0, 0);
    m_tileBatchSize = 10;
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
