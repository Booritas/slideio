// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/base/slideio_enums.hpp"

namespace slideio {
    class EncodeParameters {
    protected: 
        EncodeParameters() : m_compression(Compression::Unknown) {};
	public:
        Compression getCompression() const {
            return m_compression;
        }
    protected:
        Compression m_compression;
    };
    class JpegEncodeParameters : public EncodeParameters {
    public:
        JpegEncodeParameters(int quality = 95) {
            m_compression = Compression::Jpeg;
            m_quality = quality;
        }
        int getQuality() const {
            return m_quality;
        }
        void setQuality(int quality) {
            m_quality = quality;
        }
    private:
        int m_quality;
    };
    class JP2KEncodeParameters : public EncodeParameters {
    public:
        enum Codec {
            J2KStream,
            J2KFile
        };
        JP2KEncodeParameters(float rate = 4.5, Codec codec = Codec::J2KStream) {
            m_compression = Compression::Jpeg2000;
            m_subSamplingDX = 1;
            m_subSamplingDY = 1;
            m_codecFormat = codec;
            m_compressionRate = rate;
        }
        int getSubSamplingDx() const {
            return m_subSamplingDX;
        }
        void setSubSamplingDx(int subSamplingDx) {
            m_subSamplingDX = subSamplingDx;
        }
        int getSubSamplingDy() const {
            return m_subSamplingDY;
        }
        void setSubSamplingDy(int subSamplingDy) {
            m_subSamplingDY = subSamplingDy;
        }
        Codec getCodecFormat() const {
            return m_codecFormat;
        }
        void setCodecFormat(Codec codecFormat) {
            m_codecFormat = codecFormat;
        }
        float getCompressionRate() const {
            return m_compressionRate;
        }
        void setCompressionRate(float compressionRate) {
            m_compressionRate = compressionRate;
        }
    private:
        int m_subSamplingDX;
        int m_subSamplingDY;
        Codec m_codecFormat;
        float m_compressionRate;
    };
}
