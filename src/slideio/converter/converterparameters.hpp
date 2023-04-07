// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/imagetools/encodeparameters.hpp"

namespace slideio
{
    enum ImageFormat {
        Unknown,
        SVS
    };

    enum ConverterEncoding {
        UNKNOWN_ENCODING,
        JPEG,
        JPEG2000

    };
    class ConverterParameters
    {
    protected:
        ConverterParameters() : m_format(ImageFormat::Unknown) {
        }
    public:
        ImageFormat getFormat() const {
            return m_format;
        }
    protected:
        ImageFormat m_format;
    };

    class SVSConverterParameters : public ConverterParameters
    {
    protected:
        SVSConverterParameters() : m_tileWidth(256),
            m_tileHeight(256),
            m_numZoomLevels(-1),
            m_tileEncoding(Compression::Unknown) {
            m_format = ImageFormat::SVS;
        }
    public:
        Compression getEncoding() const {
            return m_tileEncoding;
        }
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
        virtual EncodeParameters& getEncodeParameters() = 0;
        virtual const EncodeParameters& getEncodeParameters() const = 0;
    protected:
        Compression m_tileEncoding;
        int m_tileWidth;
        int m_tileHeight;
        int m_numZoomLevels;
    };

    class SVSJpegConverterParameters :  public SVSConverterParameters,
                                        public JpegEncodeParameters
    {
    public:
        SVSJpegConverterParameters(){
            m_tileEncoding = Compression::Jpeg;
        }
        EncodeParameters& getEncodeParameters() override {
            return static_cast<JpegEncodeParameters&>(*this);
        }
        const EncodeParameters& getEncodeParameters() const override {
            return static_cast<const JpegEncodeParameters&>(*this);
        }
    };

    class SVSJp2KConverterParameters :  public SVSConverterParameters,
                                        public JP2KEncodeParameters
    {
    public:
        SVSJp2KConverterParameters() {
            m_tileEncoding = Compression::Jpeg2000;
        }
        EncodeParameters& getEncodeParameters() override {
            return static_cast<JP2KEncodeParameters&>(*this);
        }
        const EncodeParameters& getEncodeParameters() const override {
            return static_cast<const JP2KEncodeParameters&>(*this);
        }
    };
}

