// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/imagetools/encodeparameters.hpp"
#include "slideio/base/rect.hpp"
#include "slideio/base/range.hpp"
#include "slideio/base/slideio_enums.hpp"
#include "slideio/converter/converter_def.hpp"

namespace slideio
{
    class CVScene;
}

namespace slideio
{
    namespace converter
    {
        enum ImageFormat
        {
            Unknown,
            SVS,
            OME_TIFF
        };

        enum Encoding
        {
            UNKNOWN_ENCODING,
            JPEG,
            JPEG2000
        };

        enum Container
        {
            UNKNOWN_CONTAINER,
            TIFF_CONTAINER
        };

        class ContainerParameters
        {
        protected:
            ContainerParameters(Container containerType) : m_containerType(containerType) {
            };
            virtual ~ContainerParameters() = default;

        public:
            Container getContainerType() const {
                return m_containerType;
            }

        private:
            Container m_containerType;
        };

        class TIFFContainerParameters : public ContainerParameters
        {
        public:
            TIFFContainerParameters() : ContainerParameters(TIFF_CONTAINER),
                                        m_tileWidth(256),
                                        m_tileHeight(256),
                                        m_numZoomLevels(-1) {
            }

            ~TIFFContainerParameters() override = default;

            int getTileWidth() const {
                return m_tileWidth;
            }

            void setTileWidth(int tileWidth) {
                m_tileWidth = tileWidth;
            }

            int getTileHeight() const {
                return m_tileHeight;
            }

            void setTileHeight(int tileHeight) {
                m_tileHeight = tileHeight;
            }

            int getNumZoomLevels() const {
                return m_numZoomLevels;
            }

            void setNumZoomLevels(int numZoomLevels) {
                m_numZoomLevels = numZoomLevels;
            }

        protected:
            int m_tileWidth;
            int m_tileHeight;
            int m_numZoomLevels;
        };

        class SLIDEIO_CONVERTER_EXPORTS ConverterParameters
        {
        public:
            ConverterParameters(ImageFormat format, Container containerType, Compression compression);

            ConverterParameters() {
                initialize();
            }

            ConverterParameters(const ConverterParameters& other);
            ConverterParameters& operator=(const ConverterParameters& other);

            virtual ~ConverterParameters() = default;

            ImageFormat getFormat() const {
                return m_format;
            }

            const Rect& getRect() const {
                return m_rect;
            }

            void setRect(const Rect& rect) {
                m_rect = rect;
            }

            void setSliceRange(const Range& range) {
                m_sliceRange = range;
            }

            const Range& getSliceRange() const {
                return m_sliceRange;
            }

            void setChannelRange(const Range& range) {
                m_channelRange = range;
            }

            const Range& getChannelRange() const {
                return m_channelRange;
            }

            void setTFrameRange(const Range& range) {
                m_frameRange = range;
            }

            const Range& getTFrameRange() const {
                return m_frameRange;
            }

            Compression getEncoding() const;

            Container getContainerType() const;

            std::shared_ptr<EncodeParameters> getEncodeParameters() {
                return m_encodeParameters;
            }

            std::shared_ptr<const EncodeParameters> getEncodeParameters() const {
                return m_encodeParameters;
            }

            std::shared_ptr<ContainerParameters> getContainerParameters() {
                return std::static_pointer_cast<TIFFContainerParameters>(m_containerParameters);
            }

            std::shared_ptr<const ContainerParameters> getContainerParameters() const {
                return std::static_pointer_cast<TIFFContainerParameters>(m_containerParameters);
            }

            bool isValid() const {
                return m_format != ImageFormat::Unknown && m_encodeParameters != nullptr && m_containerParameters !=
                    nullptr;
            }

            void updateNotDefinedParameters(const std::shared_ptr<CVScene>& scene);

        private:
            void initialize();
            void copyFrom(const ConverterParameters& other);

        protected:
            ImageFormat m_format;
            Rect m_rect;
            Range m_sliceRange;
            Range m_channelRange;
            Range m_frameRange;
            std::shared_ptr<EncodeParameters> m_encodeParameters;
            std::shared_ptr<ContainerParameters> m_containerParameters;
        };

        class SLIDEIO_CONVERTER_EXPORTS SVSConverterParameters : public ConverterParameters
        {
        public:
            SVSConverterParameters(Compression compression)
                : ConverterParameters(ImageFormat::SVS, Container::TIFF_CONTAINER, compression) {
            }

            ~SVSConverterParameters() override = default;

            int getTileWidth() const {
                return std::static_pointer_cast<TIFFContainerParameters>(m_containerParameters)->getTileWidth();
            }

            void setTileWidth(int tileWidth) {
                std::static_pointer_cast<TIFFContainerParameters>(m_containerParameters)->setTileWidth(tileWidth);
            }

            int getTileHeight() const {
                return std::static_pointer_cast<TIFFContainerParameters>(m_containerParameters)->getTileHeight();
            }

            void setTileHeight(int tileHeight) {
                std::static_pointer_cast<TIFFContainerParameters>(m_containerParameters)->setTileHeight(tileHeight);
            }
            int getNumZoomLevels() const {
                return std::static_pointer_cast<TIFFContainerParameters>(m_containerParameters)->getNumZoomLevels();
            }

            void setNumZoomLevels(int numZoomLevels) {
                return std::static_pointer_cast<TIFFContainerParameters>(m_containerParameters)->setNumZoomLevels(numZoomLevels);
            }
        };

        class SLIDEIO_CONVERTER_EXPORTS SVSJpegConverterParameters : public SVSConverterParameters
        {
        public:
            SVSJpegConverterParameters()
                : SVSConverterParameters(Compression::Jpeg) {
            }

            ~SVSJpegConverterParameters() override = default;

            void setQuality(int q) {
                std::static_pointer_cast<slideio::JpegEncodeParameters>(m_encodeParameters)->setQuality(q);
            }

            int getQuality() const {
                return std::static_pointer_cast<slideio::JpegEncodeParameters>(m_encodeParameters)->getQuality();
            }

        };


        class SLIDEIO_CONVERTER_EXPORTS SVSJp2KConverterParameters : public SVSConverterParameters
        {
        public:
            SVSJp2KConverterParameters()
                : SVSConverterParameters(Compression::Jpeg2000) {
            }

            ~SVSJp2KConverterParameters() override = default;

            void setCompressionRate(float rate) {
                std::static_pointer_cast<slideio::JP2KEncodeParameters>(m_encodeParameters)->setCompressionRate(rate);
            }
        };

        class SLIDEIO_CONVERTER_EXPORTS OMETIFFConverterParameters : public ConverterParameters
        {
        public:
            OMETIFFConverterParameters(Compression compression)
                : ConverterParameters(ImageFormat::OME_TIFF, Container::TIFF_CONTAINER, compression) {
            }

            ~OMETIFFConverterParameters() override = default;

            int getTileWidth() const {
                return std::static_pointer_cast<TIFFContainerParameters>(m_containerParameters)->getTileWidth();
            }

            void setTileWidth(int tileWidth) {
                std::static_pointer_cast<TIFFContainerParameters>(m_containerParameters)->setTileWidth(tileWidth);
            }

            int getTileHeight() const {
                return std::static_pointer_cast<TIFFContainerParameters>(m_containerParameters)->getTileHeight();
            }

            void setTileHeight(int tileHeight) {
                std::static_pointer_cast<TIFFContainerParameters>(m_containerParameters)->setTileHeight(tileHeight);
            }

            int getNumZoomLevels() const {
                return std::static_pointer_cast<TIFFContainerParameters>(m_containerParameters)->getNumZoomLevels();
            }

            void setNumZoomLevels(int numZoomLevels) {
                return std::static_pointer_cast<TIFFContainerParameters>(m_containerParameters)->setNumZoomLevels(numZoomLevels);
            }
        };

        class SLIDEIO_CONVERTER_EXPORTS OMETIFFJpegConverterParameters : public OMETIFFConverterParameters
        {
        public:
            OMETIFFJpegConverterParameters()
                : OMETIFFConverterParameters(Compression::Jpeg) {
            }

            ~OMETIFFJpegConverterParameters() override = default;

            void setQuality(int q) {
                std::static_pointer_cast<slideio::JpegEncodeParameters>(m_encodeParameters)->setQuality(q);
            }

            int getQuality() const {
                return std::static_pointer_cast<slideio::JpegEncodeParameters>(m_encodeParameters)->getQuality();
            }
        };


        class SLIDEIO_CONVERTER_EXPORTS OMETIFFJp2KConverterParameters : public OMETIFFConverterParameters
        {
        public:
            OMETIFFJp2KConverterParameters()
                : OMETIFFConverterParameters(Compression::Jpeg2000) {
            }

            ~OMETIFFJp2KConverterParameters() override = default;

            void setCompressionRate(float rate) {
                std::static_pointer_cast<slideio::JP2KEncodeParameters>(m_encodeParameters)->setCompressionRate(rate);
            }
        };
    }
}
