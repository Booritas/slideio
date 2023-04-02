// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <memory>

class PyScene;

enum ConverterFormat {
    UNKNOWN_FORMAT,
    SVS
};

enum ConverterEncoding {
    UNKNOWN_ENCODING,
    JPEG,
    JPEG2000

};
class PyConverterParameters
{
protected:
    PyConverterParameters() : m_format(ConverterFormat::UNKNOWN_FORMAT) {
    }
public:
    ConverterFormat getFormat() const {
        return m_format;
    }
protected:
    ConverterFormat m_format;
};

class PySVSConverterParameters : public PyConverterParameters
{
protected:
    PySVSConverterParameters() :    m_tileWidth(256),
                                    m_tileHeight(256),
                                    m_numZoomLevels(-1),
                                    m_tileEncoding(UNKNOWN_ENCODING){
        m_format = ConverterFormat::SVS;
    }
    ConverterEncoding getEncoding() const {
        return m_tileEncoding;
    }
public:
    int m_tileWidth;
    int m_tileHeight;
    int m_numZoomLevels;
protected:
    ConverterEncoding m_tileEncoding;
};

class PySVSJpegConverterParameters : public PySVSConverterParameters
{
public:
    PySVSJpegConverterParameters() : m_quality(95) {
        m_tileEncoding = ConverterEncoding::JPEG;
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

class PySVSJp2KConverterParameters : public PySVSConverterParameters
{
public:
    PySVSJp2KConverterParameters() : m_compressionRate(5) {
        m_tileEncoding = ConverterEncoding::JPEG2000;
    }
    float getCompressionRate() const {
        return m_compressionRate;
    }
    void setCompressionRate(float compressionRate) {
        m_compressionRate = compressionRate;
    }
private:
    float m_compressionRate;
};

std::shared_ptr<PyConverterParameters> createConverterParameters(ConverterFormat format, ConverterEncoding encoding);
void pyConvertFile(std::shared_ptr<PyScene>& pyScene, std::shared_ptr<PyConverterParameters>&  params, const std::string& filePath);
