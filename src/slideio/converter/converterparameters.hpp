// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/imagetools/encodeparameters.hpp"
#include "slideio/base/rect.hpp"
#include "slideio/base/slideio_enums.hpp"
#include "slideio/converter/converter_def.hpp"

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
            ConverterParameters(ImageFormat format, Container containerType, slideio::Compression compression);

        public:
            virtual ~ConverterParameters() {
            }

            ImageFormat getFormat() const {
                return m_format;
            }

            Rect& getRect() {
                return m_rect;
            }

            void setRect(const Rect& rect) {
                m_rect = rect;
            }

            int32_t getZSlice() const {
                return m_zSlice;
            }

            void setZSlice(int32_t zSlice) {
                m_zSlice = zSlice;
            }

            int32_t getTFrame() const {
                return m_tFrame;
            }

            void setTFrame(int32_t tFrame) {
                m_tFrame = tFrame;
            }

            Compression getEncoding() const {
                return m_encodeParameters->getCompression();
            }

            Container getContainerType() const {
                return m_containerParameters->getContainerType();
            }

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

        protected:
            ImageFormat m_format;
            Rect m_rect;
            int32_t m_zSlice;
            int32_t m_tFrame;
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
        };
    }
}
