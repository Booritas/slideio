// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_zviscene_HPP
#define OPENCV_slideio_zviscene_HPP
#include "slideio/core/cvscene.hpp"
#include "slideio/imagetools/tilecomposer.hpp"
#include "slideio/drivers/czi/czisubblock.hpp"
#include "slideio/drivers/czi/czistructs.hpp"
#include <map>
#include <pole/storage.hpp>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class ZVISlide;
    class SLIDEIO_EXPORTS ZVIScene : public CVScene
    {
    public:
        ZVIScene(const std::string& filePath);
        std::string getFilePath() const override;
        cv::Rect getRect() const override;
        int getNumChannels() const override;
        int getNumZSlices() const override;
        int getNumTFrames() const override;
        double getZSliceResolution() const override;
        double getTFrameResolution() const override;
        slideio::DataType getChannelDataType(int channel) const override;
        std::string getChannelName(int channel) const override;
        Resolution getResolution() const override;
        double getMagnification() const override;
        void readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
            const std::vector<int>& componentIndices, cv::OutputArray output) override;
        void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
            const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output);
        void readResampled4DBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
            const std::vector<int>& channelIndices, const cv::Range& zSliceRange, const cv::Range& timeFrameRange,
            cv::OutputArray output) override;
        std::string getName() const override;
        Compression getCompression() const override;
    private:
        void init();
    private:
        std::string m_filePath;
        ole::compound_document m_Doc;
        int m_Width;
        int m_Height;
        int m_PixelFormat;
        int m_RawCount;
    };
}


#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif
