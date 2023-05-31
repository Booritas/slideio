#pragma once
#include "slideio/core/cvscene.hpp"

class TestScene : public slideio::CVScene
{
public:
    TestScene() :
        m_filePath("/path/folder/file.svs"),
        m_rect(0, 0, 100, 100),
        m_numChannels(3),
        m_dataType(slideio::DataType::DT_Unknown),
        m_name("TestScene"),
        m_resolution(1., 1.),
        m_magnification(20.0),
        m_compression(slideio::Compression::Jpeg)
    {}
    std::string getFilePath() const override { return m_filePath; }
    void setFilePath(const std::string& filePath) { m_filePath = filePath; }
    std::string getName() const override { return m_name; }
    void setName(const std::string& name) { m_name = name; }
    cv::Rect getRect() const override { return m_rect; }
    void setRect(const cv::Rect& rect) { m_rect = rect; }
    int getNumChannels() const override { return m_numChannels; }
    void setNumChannels(int numChannels) { m_numChannels = numChannels; }
    slideio::DataType getChannelDataType(int channel) const override { return m_dataType; }
    void setChannelDataType(slideio::DataType dataType) { m_dataType = dataType; }
    slideio::Resolution getResolution() const override { return m_resolution; }
    void setResolution(const slideio::Resolution& resolution) { m_resolution = resolution; }
    double getMagnification() const override { return m_magnification; }
    void setMagnification(double magnification) { m_magnification = magnification; }
    slideio::Compression getCompression() const override { return m_compression; }
    void setCompression(slideio::Compression compression) { m_compression = compression; }
private:
    std::string m_filePath;
    cv::Rect m_rect;
    int m_numChannels;
    slideio::DataType m_dataType;
    std::string m_name;
    slideio::Resolution m_resolution;
    double m_magnification;
    slideio::Compression m_compression;
};