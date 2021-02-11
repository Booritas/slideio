// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_cvsmallscene_HPP
#define OPENCV_slideio_cvsmallscene_HPP

#include "slideio/slideio_def.hpp"
#include "slideio/core/cvscene.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SLIDEIO_EXPORTS CVSmallScene : public CVScene
    {
    public:
        CVSmallScene() {
            init();
        }
        std::string getFilePath() const override {
            return m_filePath;
        }
        std::string getName() const override {
            return m_sceneName;
        }
        cv::Rect getRect() const override {
            return m_sceneRect;
        }
        int getNumChannels() const override {
            return m_numChannel;
        }
        slideio::DataType getChannelDataType(int channel) const override {
            return m_channelDataType;
        }
        Resolution getResolution() const override {
            return m_resolution;
        }
        double getMagnification() const override {
            return m_magnification;
        }
        Compression getCompression() const override {
            return m_compression;
        }
        void readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
            const std::vector<int>& channelIndices, cv::OutputArray output) override;
        virtual bool init() { return false; }
    protected:
        virtual void readImage(cv::OutputArray output) = 0;
    protected:
        std::string m_filePath;
        std::string m_sceneName;
        cv::Rect m_sceneRect{0,0,0,0};
        int m_numChannel{0};
        Resolution m_resolution{0.,0.};
        double m_magnification{ 0. };
        Compression m_compression{ Compression::Unknown };
        DataType m_channelDataType;
    };
};

#endif